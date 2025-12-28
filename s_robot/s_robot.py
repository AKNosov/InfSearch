import argparse
import hashlib
import re
import time
import threading
import urllib.parse
from concurrent.futures import ThreadPoolExecutor

import requests
import yaml
from bs4 import BeautifulSoup
from lxml import html as lxml_html
from pymongo import MongoClient, ReturnDocument


class SimpleSearchRobot:
    TRACKING_PREFIXES = ("utm_", "gclid", "fbclid", "yclid")

    LENTA_ARTICLE_RE = re.compile(r"^/news/\d{4}/\d{2}/\d{2}/[^/]+/?$")
    RBC_ARTICLE_RE = re.compile(r"^/[a-zA-Z0-9_-]+/\d{2}/\d{2}/\d{4}/.*$")

    EXCLUDE_PATH_RE = re.compile(
        r"(^/tags/|^/tag/|^/search/|^/auth/|^/user/|^/account/|^/amp/|^/video/|^/gallery/|^/photo/|^/subscribe/)"
    )

    def __init__(self, config_path: str):
        with open(config_path, "r", encoding="utf-8") as f:
            cfg = yaml.safe_load(f)

        db = cfg["db"]
        logic = cfg["logic"]
        self.seeds = cfg.get("seeds", [])

        self.delay = float(logic.get("delay_seconds", 0.7))
        self.workers = int(logic.get("workers", 4))
        self.per_source_limit = int(logic.get("per_source_limit", 20000))
        self.timeout = int(logic.get("request_timeout", 20))
        self.max_retries = int(logic.get("max_retries", 3))
        self.recrawl_after_seconds = int(logic.get("recrawl_after_seconds", 7 * 86400))
        self.non_article_refetch_seconds = int(logic.get("non_article_refetch_seconds", 120))
        self.links_per_page = int(logic.get("links_per_page", 500))

        self.client = MongoClient(db["uri"])
        self.db = self.client[db["database"]]
        self.pages = self.db[db.get("pages_collection", "pages")]
        self.queue = self.db[db.get("queue_collection", "queue")]

        self.pages.create_index("url", unique=True)
        self.pages.create_index([("source", 1), ("fetched_at", -1)])
        self.queue.create_index("url", unique=True)
        self.queue.create_index([("state", 1), ("next_fetch_at", 1), ("source", 1)])

        self._domain_lock = threading.Lock()
        self._last_req = {}

        self._count_lock = threading.Lock()
        self._saved_count = {
            "lenta.ru": self.pages.count_documents({"source": "lenta.ru"}),
            "rbc.ru": self.pages.count_documents({"source": "rbc.ru"}),
        }

        self._stop = threading.Event()

    @classmethod
    def normalize_url(cls, url: str) -> str:
        url = url.strip()
        p = urllib.parse.urlsplit(url)

        scheme = (p.scheme or "https").lower()
        netloc = p.netloc.lower()
        if netloc.startswith("www."):
            netloc = netloc[4:]

        path = p.path or "/"
        path = re.sub(r"/{2,}", "/", path)
        if path != "/" and path.endswith("/"):
            path = path[:-1]

        q = urllib.parse.parse_qsl(p.query, keep_blank_values=True)
        q = [(k, v) for k, v in q if not k.lower().startswith(cls.TRACKING_PREFIXES)]
        q.sort()
        query = urllib.parse.urlencode(q, doseq=True)

        return urllib.parse.urlunsplit((scheme, netloc, path, query, ""))

    @staticmethod
    def get_source(url: str) -> str | None:
        host = urllib.parse.urlsplit(url).netloc.lower()
        if host.endswith("rbc.ru"):
            return "rbc.ru"
        if host.endswith("lenta.ru"):
            return "lenta.ru"
        return None

    def is_article(self, url: str) -> bool:
        p = urllib.parse.urlsplit(url)
        host = p.netloc.lower()
        path = p.path or "/"

        if self.EXCLUDE_PATH_RE.search(path):
            return False

        if host.endswith("lenta.ru"):
            return bool(self.LENTA_ARTICLE_RE.match(path))
        if host.endswith("rbc.ru"):
            if host.startswith(("quote.", "trends.", "plus.")):
                return False
            return bool(self.RBC_ARTICLE_RE.match(path))
        return False

    @staticmethod
    def sha256_bytes(b: bytes) -> str:
        return hashlib.sha256(b).hexdigest()

    def extract_links_fast(self, base_url: str, html_text: str, limit: int) -> list[str]:
        try:
            doc = lxml_html.fromstring(html_text)
        except Exception:
            return []

        hrefs = doc.xpath("//a/@href")
        out, seen = [], set()

        for href in hrefs:
            if not href or href.startswith(("javascript:", "mailto:", "tel:")):
                continue
            abs_url = urllib.parse.urljoin(base_url, href)
            n = self.normalize_url(abs_url)

            if not self.get_source(n):
                continue

            path = urllib.parse.urlsplit(n).path or "/"
            if self.EXCLUDE_PATH_RE.search(path):
                continue

            if n not in seen:
                seen.add(n)
                out.append(n)
                if len(out) >= limit:
                    break

        return out

    def extract_article_text(self, url: str, html: str) -> str:
        soup = BeautifulSoup(html, "lxml")
        for tag in soup(["script", "style", "noscript", "svg", "form"]):
            tag.decompose()

        title = ""
        h1 = soup.find("h1")
        if h1:
            title = h1.get_text(" ", strip=True)

        host = urllib.parse.urlsplit(url).netloc.lower()

        candidates = []
        if host.endswith("lenta.ru"):
            candidates += [
                "div.topic-body__content",
                "div.topic-body",
                "div[data-testid='topic-body']",
                "article",
                "main",
            ]
        elif host.endswith("rbc.ru"):
            candidates += [
                "div.article__text",
                "div.article__content",
                "article",
                "main",
            ]
        else:
            candidates += ["article", "main"]

        body_node = None
        for sel in candidates:
            body_node = soup.select_one(sel)
            if body_node:
                break

        if not body_node:
            return ""

        parts = []
        for p in body_node.select("p, li"):
            txt = p.get_text(" ", strip=True)
            if not txt:
                continue
            if len(txt) < 40:
                continue
            parts.append(txt)

        body_text = "\n".join(parts).strip()
        if not body_text:
            body_text = body_node.get_text("\n", strip=True)

        if title and body_text:
            return f"{title}\n\n{body_text}".strip()
        return (title or body_text).strip()

    def seed(self):
        now = int(time.time())
        for u in self.seeds:
            nu = self.normalize_url(u)
            src = self.get_source(nu)
            if src:
                pr = 1
                self.enqueue(nu, src, next_fetch_at=now, priority=pr)

    def enqueue(self, url: str, source: str, next_fetch_at: int, priority: int) -> None:
        now = int(time.time())

        self.queue.update_one(
            {"url": url},
            [{
                "$set": {
                    "url": {"$ifNull": ["$url", url]},
                    "source": {"$ifNull": ["$source", source]},
                    "state": {"$ifNull": ["$state", "new"]},
                    "discovered_at": {"$ifNull": ["$discovered_at", now]},
                    "tries": {"$ifNull": ["$tries", 0]},
                    "next_fetch_at": {"$ifNull": ["$next_fetch_at", next_fetch_at]},
                    "priority": {"$ifNull": ["$priority", priority]},
                }
            },
            {
                "$set": {
                    "priority": {"$min": ["$priority", priority]}
                }
            }],
            upsert=True
        )

    def claim_next(self) -> dict | None:
        now = int(time.time())
        return self.queue.find_one_and_update(
            {"state": "new", "next_fetch_at": {"$lte": now}},
            {"$set": {"state": "processing", "processing_at": now}},
            sort=[("priority", 1), ("next_fetch_at", 1), ("discovered_at", 1)],
            return_document=ReturnDocument.AFTER,
        )

    def reschedule(self, url: str, is_article: bool):
        now = int(time.time())
        next_fetch = now + (self.recrawl_after_seconds if is_article else self.non_article_refetch_seconds)
        self.queue.update_one(
            {"url": url},
            {"$set": {"state": "new", "next_fetch_at": next_fetch}, "$unset": {"last_error": ""}},
        )

    def fail(self, url: str, err: str, backoff_seconds: int = 60):
        now = int(time.time())
        self.queue.update_one(
            {"url": url},
            {
                "$set": {"state": "new", "last_error": err[:4000], "next_fetch_at": now + backoff_seconds},
                "$inc": {"tries": 1},
            },
        )

    def domain_wait(self, url: str):
        domain = urllib.parse.urlsplit(url).netloc.lower()

        while True:
            with self._domain_lock:
                last = self._last_req.get(domain, 0.0)
                now = time.monotonic()
                to_sleep = self.delay - (now - last)
                if to_sleep <= 0:
                    self._last_req[domain] = time.monotonic()
                    return
            time.sleep(to_sleep)

    def save_article_if_changed(self, url: str, source: str, html: str) -> bool:
        now = int(time.time())

        html_hash = self.sha256_bytes(html.encode("utf-8", errors="ignore"))

        prev = self.pages.find_one({"url": url}, {"html_hash": 1})
        if prev and prev.get("html_hash") == html_hash:
            self.pages.update_one({"url": url}, {"$set": {"fetched_at": now, "source": source}})
            return False

        text = self.extract_article_text(url, html)
        if not text:
            return False

        update = {
            "$set": {
                "url": url,
                "source": source,
                "fetched_at": now,
                "html": html,
                "text": text,
                "html_hash": html_hash,
            }
        }
        res = self.pages.update_one({"url": url}, update, upsert=True)

        if res.upserted_id is not None:
            with self._count_lock:
                self._saved_count[source] = self._saved_count.get(source, 0) + 1

        return True

    def worker(self, worker_id: int):
        session = requests.Session()
        while not self._stop.is_set():
            try:
                job = self.claim_next()
                if not job:
                    time.sleep(0.2)
                    continue

                url = job["url"]
                source = job["source"]
                tries = int(job.get("tries", 0))
                is_art = self.is_article(url)

                self.domain_wait(url)

                r = session.get(url, timeout=(5, self.timeout), allow_redirects=True)
                if r.status_code < 200 or r.status_code >= 400:
                    raise RuntimeError(f"bad_status={r.status_code}")

                html = r.text

                if is_art:
                    self.save_article_if_changed(url, source, html)

                links = self.extract_links_fast(url, html, limit=self.links_per_page)
                now = int(time.time())
                for link in links:
                    src = self.get_source(link)
                    if not src:
                        continue
                    is_art_link = self.is_article(link)
                    pr = 0 if is_art_link else 1
                    nfa = now if is_art_link else (now + self.non_article_refetch_seconds)
                    self.enqueue(link, src, next_fetch_at=nfa, priority=pr)

                self.reschedule(url, is_article=is_art)

            except Exception as e:
                print(f"[worker {worker_id}] error: {e}", flush=True)
                try:
                    if "url" in locals():
                        if tries + 1 >= self.max_retries:
                            self.fail(url, f"max_retries: {e}", backoff_seconds=3600)
                        else:
                            self.fail(url, str(e), backoff_seconds=60)
                except Exception as ee:
                    print(f"[worker {worker_id}] fail() error: {ee}", flush=True)

    def run(self):
        self.seed()
        with ThreadPoolExecutor(max_workers=self.workers) as ex:
            futures = [ex.submit(self.worker, i) for i in range(self.workers)]
            try:
                while True:
                    for i, f in enumerate(futures):
                        if f.done():
                            print(f"[main] worker {i} died:", f.exception(), flush=True)
                            futures[i] = ex.submit(self.worker, i)
                    time.sleep(1)
            except KeyboardInterrupt:
                self._stop.set()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config", help="Path to YAML config")
    args = ap.parse_args()
    SimpleSearchRobot(args.config).run()


if __name__ == "__main__":
    main()