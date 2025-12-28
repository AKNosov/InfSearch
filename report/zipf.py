import argparse
import math
import re
from collections import Counter

import matplotlib.pyplot as plt
from pymongo import MongoClient


TOKEN_RE = re.compile(r"[a-zA-Zа-яА-ЯёЁ0-9]+", re.UNICODE)

def tokenize(text: str):
    text = text.lower().replace("ё", "е")
    toks = TOKEN_RE.findall(text)
    out = []
    for t in toks:
        if len(t) < 2:
            continue
        if t.isdigit():
            continue
        out.append(t)
    return out


def load_terms_from_mongo(uri: str, db: str, coll: str, field: str, limit: int = 0):
    client = MongoClient(uri)
    c = client[db][coll]
    cur = c.find({field: {"$type": "string"}}, {field: 1, "_id": 0})
    if limit > 0:
        cur = cur.limit(limit)
    for doc in cur:
        yield doc.get(field, "")


def fit_zipf(rank, freq, r_min=1, r_max=50000):
    r_min = max(1, r_min)
    r_max = min(len(freq), r_max)

    xs, ys = [], []
    for r in range(r_min, r_max + 1):
        f = freq[r - 1]
        if f <= 0:
            continue
        xs.append(math.log(r))
        ys.append(math.log(f))

    n = len(xs)
    if n < 2:
        return None

    x_mean = sum(xs) / n
    y_mean = sum(ys) / n
    num = sum((xs[i] - x_mean) * (ys[i] - y_mean) for i in range(n))
    den = sum((xs[i] - x_mean) ** 2 for i in range(n))
    b = num / den
    a = y_mean - b * x_mean

    s = -b
    C = math.exp(a)
    return C, s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--uri", required=True, help="Mongo URI, e.g. mongodb://localhost:27017 or mongodb://mongo:27017")
    ap.add_argument("--db", required=True)
    ap.add_argument("--coll", required=True)
    ap.add_argument("--field", default="text")
    ap.add_argument("--limit_docs", type=int, default=0, help="0 = all")
    ap.add_argument("--max_terms_plot", type=int, default=50000, help="max ranks to plot")
    ap.add_argument("--fit_r_min", type=int, default=50)
    ap.add_argument("--fit_r_max", type=int, default=20000)
    ap.add_argument("--out", default="zipf.png")
    args = ap.parse_args()

    cnt = Counter()
    docs = 0

    for text in load_terms_from_mongo(args.uri, args.db, args.coll, args.field, args.limit_docs):
        docs += 1
        cnt.update(tokenize(text))
        if docs % 1000 == 0:
            print(f"processed docs: {docs}, unique terms: {len(cnt)}")

    print(f"Total docs: {docs}")
    print(f"Unique terms: {len(cnt)}")
    if not cnt:
        print("No terms found.")
        return

    freqs = [f for _, f in cnt.most_common()]
    n_plot = min(args.max_terms_plot, len(freqs))
    ranks = list(range(1, n_plot + 1))
    y = freqs[:n_plot]

    f1 = freqs[0]
    zipf1 = [f1 / r for r in ranks]

    fit = fit_zipf(ranks, freqs, args.fit_r_min, args.fit_r_max)
    if fit:
        C, s = fit
        zipf_fit = [C * (r ** (-s)) for r in ranks]
        print(f"Fitted Zipf: f(r) = {C:.3g} * r^(-{s:.3f})  (fit ranks {args.fit_r_min}-{args.fit_r_max})")
    else:
        zipf_fit = None
        print("Not enough data to fit Zipf.")

    plt.figure(figsize=(10, 7))
    plt.loglog(ranks, y, label="Corpus term frequencies", linewidth=2)
    plt.loglog(ranks, zipf1, "--", label="Zipf f(r)=f(1)/r", linewidth=2)
    if zipf_fit:
        plt.loglog(ranks, zipf_fit, "--", label=f"Fit: C*r^(-s), s={s:.3f}", linewidth=2)

    plt.title("Zipf's law: rank-frequency (log-log)")
    plt.xlabel("Rank r")
    plt.ylabel("Frequency f(r)")
    plt.grid(True, which="both", linestyle=":", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    plt.savefig(args.out, dpi=200)
    print(f"Saved plot to {args.out}")


if __name__ == "__main__":
    main()