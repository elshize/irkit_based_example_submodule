#include <iostream>

#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/index/source.hpp>


using doc_list_t = irk::index::block_document_list_view<long>;
using payload_list_t = irk::index::block_payload_list_view<long>;
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
    for(size_t i = 0; i < query.size(); ++i){
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
    assert(argc == 2);
    irk::fs::path index_dir(argv[1]);
    std::ifstream term_in(irk::index::term_map_path(index_dir).c_str());
    std::ifstream title_in(irk::index::title_map_path(index_dir).c_str());
    irk::inverted_index_mapped_data_source data(index_dir,
                                                    irk::score::bm25_tag{});
    irk::inverted_index_view index_view(&data, irk::varbyte_codec<long>{},
                                            irk::varbyte_codec<long>{},
                                            irk::varbyte_codec<long>{});


    std::vector<int> queryID = { 5, 5936, 342325 };
    std::vector<post_list_t> query_postings;
    for(size_t i = 0; i < queryID.size(); ++i){
        query_postings.push_back(index_view.scored_postings(queryID[i]));
    }
    

    for(auto plist : query_postings){
        for(auto posting : plist){
            std::cout << posting.document() << "\t"
                      << posting.payload() << std::endl;
        }
        std::cout << std::endl;
    }
    
    int k = 15;
    auto result = taat(k, query_postings, index_view.collection_size());
    std::cout << "Top " << k << " Results\nDocument\tScore\n";
    print_posting(result);






}
