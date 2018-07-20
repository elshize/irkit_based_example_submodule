#include <iostream>

#include <irkit/index.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/index/posting_list.hpp>

using std::uint32_t;
using irk::index::document_t;
using doc_list_t = irk::index::block_document_list_view<
    irk::stream_vbyte_codec<document_t>>;
using payload_list_t = irk::index::block_payload_list_view<uint32_t,
    irk::stream_vbyte_codec<uint32_t>>;
using post_list_t = irk::posting_list_view<doc_list_t, payload_list_t>;

template<class Document, class Score>
struct Posting {
    Document document;
    Score score;
};

void print_posting(const std::vector<Posting<long, long> >& postings){
    for(auto p : postings){
        std::cout << p.document << "\t\t"
             << p.score << std::endl;
    }
}

bool order(const Posting<long, long>& lhs, 
            const Posting<long, long>& rhs)
{
    return lhs.score > rhs.score;
}

auto taat(int k, const std::vector<post_list_t>& query,
            long collection_size){
    std::vector<long> acc(collection_size, 0);
    std::vector<Posting<long, long> > topk;
    long threshold = 0;
    for(size_t i = 0; i < query.size(); ++i) {
        std::cout << "term " << i << "\n";
        for(auto posting : query[i]){
            acc[posting.document()] += posting.payload();
        }
    }
    for(size_t i = 0; i < acc.size(); ++i){
        if(acc[i] > threshold){
            Posting<long, long> p {long(i), acc[i]};
            topk.push_back(p);
            if(topk.size() <= k) std::push_heap(topk.begin(), topk.end(), order);
            else{
                std::pop_heap(topk.begin(), topk.end(), order);
                topk.pop_back();
            }
            threshold = topk.size() == k ? topk[0].score : threshold;
        }
    }
    std::sort(topk.begin(), topk.end(), order);
    return topk;
}

int main(int argc, char** argv){
    assert(argc == 4);
    irk::fs::path index_dir(argv[1]);
    std::ifstream term_in(irk::index::term_map_path(index_dir).c_str());
    std::ifstream title_in(irk::index::title_map_path(index_dir).c_str());
    irk::inverted_index_mapped_data_source data(index_dir, "bm25");
    irk::inverted_index_view index_view(&data);

    std::ifstream query_in(argv[2]);
    if (!query_in) {
		std::cerr << "Failed to open query.txt\n";
		exit(1);
	}

    std::ofstream result;
    result.open(argv[3]);

    std::string line;
    int k = 1000;
    int num = 1;
    while(getline(query_in, line)){
        std::cout << "Processing Query " << num << "......\n";
        std::stringstream aQuery(line);
        std::string term;
        std::vector<post_list_t> query_postings;
        int rank = 0;
        while(aQuery >> term){
            auto id = index_view.term_id(term);
            if (id.has_value()) {
                query_postings.push_back(
                index_view.scored_postings(id.value()));
            }
        }
        if(query_postings.size() == 0){
            result << num << "\tQ0\t" 
                   << argv[3] << "\t" 
                   << rank << "\t"
                   << "0\tnull\n";
            continue;
        }
        auto topk = taat(k, query_postings, index_view.collection_size());
        for(; rank < topk.size(); ++rank){
            result << num << "\tQ0\t"
                << index_view.titles().key_at(topk[rank].document) << "\t"
                << rank << "\t"
                << topk[rank].score << "\tnull\n"  << std::flush;
        }
        ++num;
    }
}