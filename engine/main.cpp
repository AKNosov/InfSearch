#include <iostream>
#include <string>
#include <vector>
#include <chrono>

#include "b_idx.h"
#include "b_srch.h"

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/options/find.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

static mongocxx::instance g_mongo_instance{};

struct MongoConfig {
    std::string uri;       
    std::string database; 
    std::string collection; 
    std::string urlField = "url";
    std::string textField = "text";
    int64_t limit = 0;      
}

static int loadAndIndexMongo(const MongoConfig& cfg, BooleanIndex& index, std::vector<std::string>& urls) {
    mongocxx::client client{ mongocxx::uri{cfg.uri} };
    auto coll = client[cfg.database][cfg.collection];

    auto filter = make_document(
        kvp(cfg.textField, make_document(kvp("$type", "string"))),
        kvp(cfg.urlField,  make_document(kvp("$type", "string")))
    );

    mongocxx::options::find opts;
    opts.projection(make_document(
        kvp(cfg.urlField, 1),
        kvp(cfg.textField, 1),
        kvp("_id", 0)
    ));
    if (cfg.limit > 0) opts.limit(cfg.limit);

    int docId = 0;
    auto cursor = coll.find(filter.view(), opts);

    for (auto&& d : cursor) {
        auto itUrl = d.find(cfg.urlField);
        auto itTxt = d.find(cfg.textField);
        if (itUrl == d.end() || itTxt == d.end()) continue;
        if (itUrl->type() != bsoncxx::type::k_utf8) continue;
        if (itTxt->type() != bsoncxx::type::k_utf8) continue;

        std::string url = itUrl->get_utf8().value.to_string();
        std::string text = itTxt->get_utf8().value.to_string();
        if (text.empty()) continue;

        urls.push_back(url);

        Document doc;
        doc.id = docId;
        doc.key = url;
        doc.text = text;

        index.addDocument(doc);
        docId++;

        if (docId % 2000 == 0) {
            std::cerr << "Indexed docs: " << docId << "\r" << std::flush;
        }
    }

    std::cerr << "\nFinalize index...\n";
    index.finalize();
    return docId;
}

static void usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " <mongo_uri> <db> <collection> [limit]\n\n"
        << "Examples:\n"
        << "  " << prog << " mongodb://mongo:27017 crawler pages\n"
        << "  " << prog << " mongodb://localhost:27017 crawler pages 50000\n";
}

int main(int argc, char** argv) {
    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }

    MongoConfig cfg;
    cfg.uri = argv[1];
    cfg.database = argv[2];
    cfg.collection = argv[3];
    if (argc >= 5) cfg.limit = std::stoll(argv[4]);

    BooleanIndex index;
    std::vector<std::string> urls;
    urls.reserve(cfg.limit > 0 ? (size_t)cfg.limit : 50000);

    auto t0 = std::chrono::steady_clock::now();
    int n = loadAndIndexMongo(cfg, index, urls);
    auto t1 = std::chrono::steady_clock::now();

    double sec = std::chrono::duration<double>(t1 - t0).count();
    std::cerr << "Indexed: " << n << " docs\n";
    std::cerr << "Index build time: " << sec << " sec\n";
    if (sec > 0) std::cerr << "Speed: " << (n / sec) << " docs/sec\n";

    BooleanSearch search(index);

    std::cout << "Boolean search ready.\n";
    std::cout << "Syntax: AND OR NOT, parentheses. Implicit AND between terms.\n";
    std::cout << "Examples:\n";
    std::cout << "  нефть AND газ\n";
    std::cout << "  (нефть OR газ) AND NOT европа\n";
    std::cout << "Ctrl+D to exit.\n";

    std::string q;
    while (std::cout << "> " && std::getline(std::cin, q)) {
        auto hits = search.search(q);
        std::cout << "hits: " << hits.size() << "\n";

        size_t k = hits.size() < 20 ? hits.size() : 20;
        for (size_t i = 0; i < k; i++) {
            int id = hits[i];
            if (id >= 0 && (size_t)id < urls.size()) {
                std::cout << "  " << urls[id] << "\n";
            }
        }
        if (hits.size() > k) {
            std::cout << "  ... (" << (hits.size() - k) << " more)\n";
        }
    }

    return 0;
}