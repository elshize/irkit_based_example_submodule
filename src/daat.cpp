#include <iostream>
#include <chrono>
#include <string>
#include <sstream>

#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/index/posting_list.hpp>
// #define private public


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

bool order(const Posting<long, long>& lhs, 
            const Posting<long, long>& rhs)
{
    return lhs.score > rhs.score;
}
// auto taat(int k, const std::vector<post_list_t>& query,
//             long collection_size){
//     std::vector<long> acc(collection_size, 0);
//     std::vector<Posting<long, long> > topk;
//     long threshold = 0;
//     for(size_t i = 0; i < query.size(); ++i){
//         for(auto posting : query[i]){
//             acc[posting.document()] += posting.payload();
//         }
//     }
//     for(size_t i = 0; i < acc.size(); ++i){
//         if(acc[i] > threshold){
//             Posting<long, long> p {long(i), acc[i]};
//             topk.push_back(p);
//             if(topk.size() <= k) std::push_heap(topk.begin(), topk.end(), order);
//             else{
//                 std::pop_heap(topk.begin(), topk.end(), order);
//                 topk.pop_back();
//             }
//             threshold = topk.size() == k ? topk[0].score : threshold;
//         }
//     }
//     std::sort(topk.begin(), topk.end(), order);
//     return topk;
// }
auto daat(int k, const std::vector<post_list_t>& query,
          long collection_size){
    std::vector<post_list_t::iterator> pointers;
    auto cur_posting = query[0].begin().document();
    std::vector<Posting<long, long> > topk;
    long threshold = 0;
    // for(auto plist : query){
    //     for(auto p : plist){
    //         std::cout << p.document() << "\t"
    //                   << p.payload() << std::endl;
    //     }
    //     std::cout << std::endl;
    // }
    for(const auto& plist : query){
        pointers.push_back(plist.begin());
        if(plist.begin().document() < cur_posting){
            cur_posting = plist.begin().document();
        }
    }
    // for(const auto& plist : query){
    //     auto p = plist.begin().nextgeq(document_t(60000));
    //     std::cout << p.document_iterator_.block() << " "
    //               << p.document_iterator_.pos() << "\t";
    //     std::cout << plist.end().document_iterator_.block()<< " "
    //               << plist.end().document_iterator_.pos() << "\n";
    // }
    // std::cout << cur_posting << std::endl;
    // std::cout << pointers[0].document() << "\t" 
    //           << query[0].begin().document() << std::endl;
    int flag = 0;
    long cur_score = 0;
    int debug = 0;
    while(flag < pointers.size()){
        flag = 0;
        cur_score = 0;
        document_t min = document_t(collection_size);
        // std::cout << debug++ << std::endl;
        // std::cout << "hey ";
        for(size_t i = 0; i < pointers.size(); ++i){
            // std::cout << i << " ";
            if(pointers[i] != query[i].end()){
                //std::cout << pointers[i].document() << '\t';
                if(pointers[i].document() == cur_posting){
                    //std::cout << "debug" << "\t";
                    cur_score += pointers[i].payload();
                    pointers[i].moveto(cur_posting + 1);
                }
                // std::cout << pointers[i].document_iterator_.block() << " "
                //               << pointers[i].document_iterator_.pos() << " ";
                if(pointers[i] != query[i].end()){
                    min = std::min(min, pointers[i].document());
                    // std::cout << pointers[i].document() << "\t";
                }
            }
            else{
                ++flag;
            }
        }
        if(cur_score > threshold){
            Posting<long, long> p {long(cur_posting), cur_score};
            topk.push_back(p);
            if(topk.size() <= k){
                std::push_heap(topk.begin(), topk.end(), order);
            }
            else{
                std::pop_heap(topk.begin(), topk.end(), order);
                topk.pop_back();
            }
            threshold = topk.size() == k ? topk[0].score : threshold;
        }
        cur_posting = min;
        // std::cout << flag << "\t";
        // std::cout << cur_posting << std::endl;
    }
    std::sort(topk.begin(), topk.end(), order);
    return topk;
}

int main(int argc, char** argv){
    assert(argc == 3);
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
    
    std::string line;
    int k = 1000;
    int num = 1;

    
    while(getline(query_in, line)){
        std::stringstream aQuery(line);
        std::string term;
        std::vector<post_list_t> query_postings;
        while(aQuery >> term){
            auto id = index_view.term_id(term);
            if (id.has_value()) {
                query_postings.push_back(
                index_view.scored_postings(id.value()));
            }
        }
        if(query_postings.size() == 0){
            std::cout << "Can't find any result for query '"
                      << line << "'\n";
            continue;
        }
        auto result = daat(k, query_postings, index_view.collection_size());
        // auto check =  taat(k, query_postings, index_view.collection_size());
        // for(size_t i = 0; i < result.size(); ++i){
        //     assert(result[i].document == check[i].document);
        //     assert(result[i].score == check[i].score);
        // }
        std::cout << "Query " << num << ": " << line 
                  << "\nDID\t" << "Score\n";  
        for(auto posting : result){
            std::cout << posting.document << "\t"
                      << posting.score << std::endl;
        }
        std::cout << std::endl;

        ++num;
    }
}