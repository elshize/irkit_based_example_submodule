#include <iostream>
#include <chrono>
#include <bitset>
#include <string>
#include <sstream>

#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/parsing/stemmer.hpp>
//#define private public
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
    for(size_t i = 0; i < query.size(); ++i){
        for(const auto& posting : query[i]){
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

auto block_max(const std::vector<post_list_t>& query, 
                long collection_size, int block_size){
    std::vector<std::vector<long> > maxTable;
    for(const auto& pList : query){
        std::vector<long> bmax(collection_size / block_size + 1, 0);
        auto p = pList.begin();
        for(size_t i = 0; i < bmax.size(); ++i){
            while(p != pList.end() && p.document() < (i + 1) * block_size){
                if(p.payload() > bmax[i]) {bmax[i] = p.payload();}
                p.moveto(p.document() + 1);
            }
        }
        maxTable.push_back(bmax);
    }
    return maxTable;
}

auto posting_bitset(const std::vector<post_list_t>& query,
                    long block_pos, int block_size){
    int subsize = block_size / 8;
    document_t startpos = block_pos * block_size;
    std::bitset<8> result(std::string("11111111"));
    for(size_t i = 0; i < query.size(); ++i){
        auto p = query[i].begin();
        std::bitset<8> pb(std::string("00000000"));
        for(size_t sub = 0; sub < 8; ++sub){
            if(p.moveto(document_t(startpos + sub * subsize)) != query[i].end()
                && p.moveto(document_t(startpos + sub * subsize)).document() 
                < startpos + (sub + 1) * subsize)
                { pb.set(sub); } 
        }
        result &= pb;
    }
    return result;
}

auto live_block_count(const std::vector<post_list_t>& query,
                      const std::vector<std::vector<long> >& maxTable, 
                      long threshold, int block_size){
    long liveBlock = 0;
    long liveBlockSub = 0;
    long liveSub = 0;
    std::vector<double> result;
    std::bitset<8> empty(std::string("00000000"));
    for(size_t i = 0; i < maxTable[0].size(); ++i){
        long temp_score = 0;
        for(const auto& bmax : maxTable){
            temp_score += bmax[i];
        }
        if(temp_score >= threshold){
            ++liveBlock;
            std::bitset<8> pb = posting_bitset(query, i, block_size);
            if(pb != empty){
                ++liveBlockSub;
                liveSub += pb.count();
            }
        } 
    }
    double rateBlock = (double)liveBlock / maxTable[0].size() * 100;
    double rateBlockSub = (double)liveBlockSub / maxTable[0].size() * 100;
    double rateSub = (double)liveSub / (maxTable[0].size() * 8) * 100;
    result.push_back(rateBlock);
    result.push_back(rateBlockSub);
    result.push_back(rateSub);
    return result;
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

    std::string line;
    int k = std::stoi(argv[3]);
    int num = 1;
    double sumRough = 0;
    double sumAdvanced = 0;
    double sumSub = 0;
    std::cout << "Query\t"
              << "Rough Rate\t" 
              << "Advanced Rate\t"
              << "Sub Blocks Rate\n";
    while(getline(query_in, line)){
        std::stringstream aQuery(line);
        std::string term;
        std::vector<post_list_t> query_postings;
        irk::porter2_stemmer stemmer;
        while(aQuery >> term){
            term = stemmer.stem(term);
            auto id = index_view.term_id(term);
            if (id.has_value()) {
                query_postings.push_back(
                index_view.scored_postings(id.value()));
            }
        }
        if(query_postings.size() == 0){
            std::cout << num++ << "\tCan't find any result for query '"
                      << line << "'\n";
            continue;
        }
        // std::chrono::nanoseconds elapsed(0);
        // auto start_interval = std::chrono::steady_clock::now();
        auto result = taat(k, query_postings, index_view.collection_size());
        // auto end_interval = std::chrono::steady_clock::now();
        // elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>
        //             (end_interval - start_interval);
        // long time = 
        //     std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        // std::cout << "Top " << k << " Results in " 
        //             << time << " ms\n"
        //             << "Document\tScore\n";
        // print_posting(result);
        long threshold = (result.end() - 1)->score;
        auto maxTable = block_max(query_postings, index_view.collection_size(), 64);
        std::vector<double> live_rate = live_block_count(query_postings, maxTable, threshold, 64);
        std::cout << num++ << "\t"
                << live_rate[0] << "%\t"
                << live_rate[1] << "%\t"
                << live_rate[2] << "%" << std::endl;
        // std::cout << "Query " << num++ << ": " << line 
        //           << "\nDID\t" << "Score\n"; 
        // for(auto posting : result){
        //     std::cout << posting.document << "\t"
        //               << posting.score << std::endl;
        // }
        // std::cout << std::endl;

        sumRough += live_rate[0];
        sumAdvanced += live_rate[1];
        sumSub += live_rate[2];
    }
    
    std::cout << "Average for " << --num << " Queries:\n"
              << "Average Rough Live Rate: " << sumRough / num << "%"
              << "\nAverage Advanced Live Rate: " << sumAdvanced / num << "%"
              << "\nAverage Sub Blocks Live Rate: " << sumSub / num << "%" << std::endl;


}