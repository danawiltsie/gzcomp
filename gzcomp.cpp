/* 
   gzcomp.cpp
   Dana Wiltsie - 06/15/2020
*/
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <list>
#include <iterator>
#include <cassert>
#include <map>
#include <string>
#include "output_stream.hpp"

// To compute CRC32 values, we can use this library
// from https://github.com/d-bahr/CRCpp
#define CRCPP_USE_CPP11
#include "CRC.h"

#include <queue>

// BELOW is huffman tree code from https://www.geeksforgeeks.org/huffman-coding-greedy-algo-3/ adapted for use here. 
// I use it to compute just the lengths

// A Huffman tree node 
struct MinHeapNode { 

	// One of the input characters 
	int data; 

	// Frequency of the character 
	unsigned freq; 

	// Left and right child 
	MinHeapNode *left, *right; 

	MinHeapNode(int data, unsigned freq) 

	{ 

		left = right = NULL; 
		this->data = data; 
		this->freq = freq; 
	} 
}; 

// For comparison of 
// two heap nodes (needed in min heap) 
struct compare { 

	bool operator()(MinHeapNode* l, MinHeapNode* r) 

	{ 
		return (l->freq > r->freq); 
	} 
}; 

// Prints huffman codes from 
// the root of Huffman Tree. 
void printCodes(struct MinHeapNode* root, int height, std::vector<u32>& result) 
{ 

	if (!root) 
		return; 


	if (root->data != -1) {
        if(height == 0){
            result[root->data] = 1;
        } else {
            result[root->data] = height;
        }
	}
		 

	printCodes(root->left, height + 1, result); 
	printCodes(root->right, height + 1, result); 
} 

// The main function that builds a Huffman Tree and 
// print codes by traversing the built Huffman Tree 
void HuffmanCodes(int freq[], int size, std::vector<u32>& result) 
{ 
	struct MinHeapNode *left, *right, *top; 

	// Create a min heap & inserts all characters of data[] 
	std::priority_queue<MinHeapNode*, std::vector<MinHeapNode*>, compare> minHeap; 

	for (int i = 0; i < size; ++i) {
        if(freq[i] != 0){
            minHeap.push(new MinHeapNode(i, freq[i]));
        }
    } 

	// Iterate while size of heap doesn't become 1 
	while (minHeap.size() != 1) { 

		// Extract the two minimum 
		// freq items from min heap 
		left = minHeap.top(); 
		minHeap.pop(); 

		right = minHeap.top(); 
		minHeap.pop(); 

		// Create a new internal node with 
		// frequency equal to the sum of the 
		// two nodes frequencies. Make the 
		// two extracted node as left and right children 
		// of this new node. Add this node 
		// to the min heap '$' is a special value 
		// for internal nodes, not used 
		top = new MinHeapNode(-1, left->freq + right->freq); 

		top->left = left; 
		top->right = right; 

		minHeap.push(top); 
	} 

	// Print Huffman codes using 
	// the Huffman tree built above 
	printCodes(minHeap.top(), 0, result); 
} 

//BELOW is the rest of the project

struct LenDist {
    u32 length;
    u32 distance;
};

const int THRESHOLD = 250;
const int MAX_BLOCK_SIZE = 800000;
const int MAX_CODE_LENGTH = 15;
const int CL_TABLE_SIZE = 19;
const int SS_TABLE_SIZE = 286;
const int DIST_TABLE_SIZE = 30;
const int SYMBOLS_ARRAY_INIT_VAL = 300;
const int DIST_SYMBOLS_ARRAY_INIT_VAL = 32770;
const int MAX_BACKREF_DIST = 32768;


struct Symbol {
    u32 value;
    u32 offset;
    u32 offbits;
    bool isLength;
};

Symbol symbols [SYMBOLS_ARRAY_INIT_VAL];
int symbolCounts [SS_TABLE_SIZE];
Symbol distSymbols [DIST_SYMBOLS_ARRAY_INIT_VAL];
int distCounts [DIST_TABLE_SIZE];
int clCounts [CL_TABLE_SIZE];

std::vector< u32 > construct_canonical_code( std::vector<u32> const & lengths ){

    unsigned int size = lengths.size();
    std::vector< unsigned int > length_counts(16,0); //Lengths must be less than 16 for DEFLATE
    u32 max_length = 0;
    for(auto i: lengths){
        assert(i <= 15);
        length_counts.at(i)++;
        max_length = std::max(i, max_length);
    }
    length_counts[0] = 0; //Disregard any codes with alleged zero length

    std::vector< u32 > result_codes(size,0);

    //The algorithm below follows the pseudocode in RFC 1951
    std::vector< unsigned int > next_code(size,0);
    {
        //Step 1: Determine the first code for each length
        unsigned int code = 0;
        for(unsigned int i = 1; i <= max_length; i++){
            code = (code+length_counts.at(i-1))<<1;
            next_code.at(i) = code;
        }
    }
    {
        //Step 2: Assign the code for each symbol, with codes of the same length being
        //        consecutive and ordered lexicographically by the symbol to which they are assigned.
        for(unsigned int symbol = 0; symbol < size; symbol++){
            unsigned int length = lengths.at(symbol);
            if (length > 0) {
                result_codes.at(symbol) = next_code.at(length)++;
            }
        }  
    } 
    return result_codes;
}

//gzip has a peculier but interesting way to represent the symbols and their distances generated
//by the algorithm. Unfortuately this means that the code to genenerate such tables is a little ugly.
void init_symbol_table() {
    u32 offset_bits = 0;
    int items_placed = 0;
    u32 index = 3;
    for(u32 i = 257; i <= 284; i++){
        for(u32 j = 0; j < (u32)(1 << offset_bits); j++) {
            symbols[index] = Symbol{i, j, offset_bits, true};    
            index++;
        }
        items_placed++;
        if (i >= 264 && items_placed >= 4) {
            offset_bits++;
            items_placed = 0;
        }
    }
    symbols[258] = Symbol{285,0,0,true};
}

void init_distance_table() {
    u32 offset_bits = 0;
    int items_placed = 0;
    u32 index = 1;
    distSymbols[0] = Symbol{0, 0, 0, false};
    for(u32 i = 0; i <= 29; i++){
        for(u32 j = 0; j < (u32)(1 << offset_bits); j++) {
            distSymbols[index] = Symbol{i, j, offset_bits, false};    
            index++;
        }
        items_placed++;
        if (i >= 3 && items_placed >= 2) {
            offset_bits++;
            items_placed = 0;
        }
    }
}

//This function take a list of lengths generated by a huffman tree, and modifies it to 
//enforce a maximum length while still maintaining the properties of the huffman code.
void enforceMaxLength(std::vector<u32>& result, int size, u32 MAX_LENGTH){
    while (1){
        int index1 = -1;
        int index2 = -1;
        u32 maxlen = MAX_LENGTH;
        for (int i = 0; i < size; i++) {
            if (result[i] > maxlen){
                index1 = i;
                index2 = -1;
                maxlen = result[i];
            }
            else if (result[i] == maxlen) {
                index2 = i;
            }
        }
        if (index1 == -1){
            break;
        }

        //find node as close to limit as possible
        int swapi = -1;
        u32 level = 0;
        for (int i = 0; i < size; i++) {
            if(result[i] > level && result[i] < MAX_LENGTH) {
                swapi = i;
                level = result[i];
            }
        }
        result[swapi]++;
        result[index1] = result[swapi];
        result[index2]--;
    }
}

struct CLSymbol {
    u32 value;
    u32 offset;
    u32 numbits;
};

void write_non_zero_cl(std::list<CLSymbol>& clsymbols, u32 count, u32 const & last_seen) {
    while(count >= 6) {
        clsymbols.push_back(CLSymbol{16, 3, 2});
        clCounts[16]++;
        count = count - 6;
    }
    if (count >= 3) {
        u32 num = count - 3;
        clsymbols.push_back(CLSymbol{16, num, 2});
        clCounts[16]++;
        count = 0;
    }
    while(count > 0) {
        clsymbols.push_back(CLSymbol{last_seen, 0, 0});
        clCounts[last_seen]++;
        count--;
    }
}

void write_zero_cl(std::list<CLSymbol>& clsymbols, u32 count) {
    if (count >= 11) {
        clsymbols.push_back(CLSymbol{18, count - 11, 7});
        clCounts[18]++;
        count = 0;
    }
    if (count >= 3) {
        u32 num = count - 3;
        clsymbols.push_back(CLSymbol{17, num, 3});
        clCounts[17]++;
        count = 0;
    }
    while(count > 0){
        clsymbols.push_back(CLSymbol{0, 0, 0});
        clCounts[0]++;
        count--;
    }
}

void write_cl_symbol_stream(std::vector<u32>& code_lengths, int size, std::list<CLSymbol>& clsymbols){
    //compute CL stream
        u32 last_seen = 16;
        u32 count = 0;
        int i = 0;
        while(i < size) {
            while(i < size && code_lengths[i] == 0) {
                if (last_seen != 0){
                    write_non_zero_cl(clsymbols, count, last_seen);
                    count = 0;
                    last_seen = 0;
                }
                count++;
                if(count == 138) {
                    clsymbols.push_back(CLSymbol{18, 138 - 11, 7});
                    clCounts[18]++;
                    count = 0;
                }
                i++;
            }
            while(i < size && code_lengths[i] != 0) {
                //std::cout << "curr:" << code_lengths[i] << "\n";
                if(last_seen == 0) {
                    write_zero_cl(clsymbols, count);
                    clsymbols.push_back(CLSymbol{code_lengths[i], 0, 0});
                    clCounts[code_lengths[i]]++;
                    count = 0;
                    last_seen = code_lengths[i];
                } else if (last_seen != code_lengths[i]){
                    write_non_zero_cl(clsymbols, count, last_seen);
                    clsymbols.push_back(CLSymbol{code_lengths[i], 0, 0});
                    clCounts[code_lengths[i]]++;
                    count = 0;
                    last_seen = code_lengths[i];
                } else {
                    count++;
                }
                i++;
            }
        }
        if(count > 0) {
            if(last_seen == 0) {
                write_zero_cl(clsymbols, count);
            } else {
                write_non_zero_cl(clsymbols, count, last_seen);
            }
        }
}

void write_block(OutputBitStream& stream, std::list<Symbol>& output, bool is_last, int type){
    stream.push_bit(is_last?1:0); //1 = last block

    //We will construct placeholder LL and distance codes
    std::vector<u32> ll_code_lengths {};
    std::vector<u32> dist_code_lengths {};

    if (type == 1) {
        stream.push_bits(1, 2); //Two bit block type (in this case, block type 1)
        //Construct a basic code with 0 - 225 having length 8 and 226 - 285 having length 9
        //(This will satisfy the Kraft-McMillan inequality exactly, and thereby fool gzip's
        // detection process for suboptimal codes)
        for(unsigned int i = 0; i <= 143; i++)
            ll_code_lengths.push_back(8);
        for(unsigned int i = 144; i <= 255; i++)
            ll_code_lengths.push_back(9);
        for(unsigned int i = 256; i <= 279; i++)
            ll_code_lengths.push_back(7);
        for(unsigned int i = 280; i <= 287; i++)
            ll_code_lengths.push_back(8);

        //Construct a distance code similarly, with 0 - 1 having length 4 and 2 - 29 having length 5
        //(This is irrelevant since we don't actually use distance codes in this example)
        for(unsigned int i = 0; i <= 29; i++)
            dist_code_lengths.push_back(5);
    } else {
        //type 2
        stream.push_bits(2, 2); //Two bit block type (in this case, block type 2)
        
        std::vector<u32> result(SS_TABLE_SIZE, 0);
        symbolCounts[256]++; //end of file symbol occurs once
        HuffmanCodes(symbolCounts, SS_TABLE_SIZE, result);
        enforceMaxLength(result, SS_TABLE_SIZE, MAX_CODE_LENGTH);
        ll_code_lengths = result;


        std::vector<u32> result2(DIST_TABLE_SIZE, 0);
        HuffmanCodes(distCounts, DIST_TABLE_SIZE, result2);
        enforceMaxLength(result2, DIST_TABLE_SIZE, MAX_CODE_LENGTH);
        dist_code_lengths = result2;

        int numSym = ll_code_lengths.size();
        for(int i = ll_code_lengths.size() -1; i >= 0 && ll_code_lengths.at(i) == 0; i--) {
            numSym--;
        }

        unsigned int HLIT = numSym - 257;

        int numDistSym = dist_code_lengths.size();
        for(int i = dist_code_lengths.size() -1; i >= 0 && dist_code_lengths.at(i) == 0; i--) {
            numDistSym--;
        }

        std::list<CLSymbol> clsymbols;
        write_cl_symbol_stream(ll_code_lengths, numSym, clsymbols);
        write_cl_symbol_stream(dist_code_lengths, numDistSym, clsymbols);

        std::vector<u32> result3(CL_TABLE_SIZE, 0);
        HuffmanCodes(clCounts, CL_TABLE_SIZE, result3);
        enforceMaxLength(result3, CL_TABLE_SIZE, 7);

        std::vector<u32> cl_code_lengths = result3;
        auto cl_code = construct_canonical_code(cl_code_lengths);

        //Variables are named as in RFC 1951
        assert(ll_code_lengths.size() >= 257); //There needs to be at least one use of symbol 256, so the ll_code_lengths table must have at least 257 elements

        unsigned int HDIST = 0;
        if (dist_code_lengths.size() == 0){
            //Even if no distance codes are used, we are required to encode at least one.
        }else{
            HDIST = numDistSym - 1;
        }
        
        std::vector<u32> cl_permutation {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

        unsigned int HCLEN = 19; 
        int numClSym = cl_permutation.size();
        for (unsigned int i = cl_permutation.size() - 1; i >= 0 && cl_code_lengths.at(cl_permutation.at(i)) == 0; i--){
            numClSym--;
        }
        HCLEN = numClSym - 4;

        //Push HLIT, HDIST and HCLEN. These are all numbers so Rule #1 applies
        stream.push_bits(HLIT, 5);
        stream.push_bits(HDIST,5);
        stream.push_bits(HCLEN,4);

        //The lengths are written in a strange order, dictated by RFC 1951
        //(This seems like a sadistic twist of the knife, but there is some amount of weird logic behind the ordering)

        //Now push each CL code length in 3 bits (the lengths are numbers, so Rule #1 applies)
        for (unsigned int i = 0; i < HCLEN+4; i++)
            stream.push_bits(cl_code_lengths.at(cl_permutation.at(i)),3); 

        for(auto it = clsymbols.begin(); it != clsymbols.end(); it++) {
            auto code = cl_code[(*it).value];
            auto bits = cl_code_lengths[(*it).value];

            for(int i = bits-1; i >= 0; i--)
                stream.push_bit((code>>(unsigned int)i)&1);
            
            if((*it).numbits != 0) {
                stream.push_bits((*it).offset, (*it).numbits);
            }
        }
    }

    auto ll_code = construct_canonical_code(ll_code_lengths);
    auto dist_code = construct_canonical_code(dist_code_lengths);

    bool dist = false;
    for(auto iter = output.begin(); iter != output.end(); iter++){
        if((*iter).isLength) {
            dist = true;
            Symbol l = (*iter);
            //write symbol
            u32 bits = ll_code_lengths.at(l.value);
            u32 code = ll_code.at(l.value);
            for(int i = bits-1; i >= 0; i--)
                stream.push_bit((code>>(unsigned int)i)&1);
            //write offset
            bits = l.offbits;
            code = l.offset;
            stream.push_bits(code, bits);
        } else if (dist) {
            dist = false;
            
            Symbol l = (*iter);
            //write symbol
            u32 bits = dist_code_lengths.at(l.value);
            u32 code = dist_code.at(l.value);
                
            for(int i = bits-1; i >= 0; i--)
                stream.push_bit((code>>(unsigned int)i)&1);
            //write offset
            bits = l.offbits;
            code = l.offset;
            stream.push_bits(code, bits);
            
        } else {
            //write symbol
            Symbol l = (*iter);
            
            u32 bits = ll_code_lengths.at(l.value);

            u32 code = ll_code.at(l.value);

            for(int i = bits-1; i >= 0; i--)
                stream.push_bit((code>>(unsigned int)i)&1);
        }
    }

    //Throw in a 256 (EOB marker)
    
    u32 symbol = 256;
    u32 bits = ll_code_lengths.at(symbol);
    u32 code = ll_code.at(symbol);
    for(int i = bits-1; i >= 0; i--)
        stream.push_bit((code>>(unsigned int)i)&1);
}

int main(){

    //See output_stream.hpp for a description of the OutputBitStream class
    OutputBitStream stream {std::cout};

    init_symbol_table();
    init_distance_table();

    //Pre-cache the CRC table
    auto crc_table = CRC::CRC_32().MakeTable();

    //Push a basic gzip header
    stream.push_bytes( 0x1f, 0x8b, //Magic Number
        0x08, //Compression (0x08 = DEFLATE)
        0x00, //Flags
        0x00, 0x00, 0x00, 0x00, //MTIME (little endian)
        0x00, //Extra flags
        0x03 //OS (Linux)
    );
    

    //Note that the types u8, u16 and u32 are defined in the output_stream.hpp header
    u32 bytes_read {0};

    char next_byte {}; //Note that we have to use a (signed) char here for compatibility with istream::get()
    //We need to see ahead of the stream by one character (e.g. to know, once we fill up a block,
    //whether there are more blocks coming), so at each step, next_byte will be the next byte from the stream

    //Keep a running CRC of the data we read.
    u32 crc {};
    std::list<u8> buffer;
    std::list<Symbol> output;
    std::string s {};

    //we maintain a map where the key is 3 character strings and the value is a list of iterators that begin with those three characters,
    //this means finding backreferences is as easy as looking up the first three characters in the buffer in the map
    std::unordered_map<std::string, std::list<std::list<u8>::iterator>> m; 

    std::cin.get(next_byte);
    bytes_read++;
    crc = CRC::Calculate(&next_byte,1, crc_table); //Add the character we just read to the CRC (even though it is not in a block yet)
    buffer.push_back(next_byte);

    //load the look aheads into the input buffer
    while (buffer.size() < 258 && std::cin.get(next_byte)) {
        crc = CRC::Calculate(&next_byte,1, crc_table, crc); //Add the character we just read to the CRC (even though it is not in a block yet)
        buffer.push_back(next_byte);
        bytes_read++;
    }

    //LZSS encoding algorithm
    std::list<u8>::iterator current = buffer.begin();
    u32 count;
    LenDist best;//store the best backreference we can find, or one that is good enough
    while (1) {
        std::list<u8>::iterator c{current};
        best = LenDist{0,0};
        if(buffer.size() >= 4) {
            std::list<u8>::iterator x {current};
            std::string key {3, 'a'};
            for(auto it = key.begin(); it != key.end(); it++) {
                *it = *x;
                x++;
            }
            auto li = m.find(key);//check the map to see if we have seen a suitable 3 character substring to indicate a good backreference
            if(li != m.end()){
                //we found some, check them all and find one that is good enough, or just use the best one we find.
                std::list<u8>::iterator currBest;
                u32 currBestCount = 0;
                for(auto listiterator = (*li).second.begin(); listiterator != (*li).second.end(); listiterator++) {
                    std::list<u8>::iterator temp {(*listiterator)};
                    std::list<u8>::iterator cur {current};
                    count = 0;
                    while(*temp == *cur && cur != buffer.end()) {
                        count++;
                        temp++;
                        cur++;
                    }
                    if(count > currBestCount) {
                        std::list<u8>::iterator temp2 {(*listiterator)};
                        currBest = temp2;
                        currBestCount = count;
                        if(currBestCount >= THRESHOLD){
                            break;
                        }
                    }
                }
                u32 dist = 0;
                while(currBest != current){
                    currBest++;
                    dist++;
                }
                LenDist r {currBestCount,dist};
                best = r;

            }

        }

        int chars_to_add = 1; // no matter what we will need to add one char to the input buffer
        if(best.length > 2) {
            // we found a backreference, add the length and distance
            Symbol s = symbols[best.length];
            symbolCounts[s.value]++;
            output.push_back(s);

            Symbol d = distSymbols[best.distance];
            distCounts[d.value]++;
            output.push_back(d);

            chars_to_add = best.length;// we need to add to the buffer as many characters as we consume
        } else {
            //no good back reference, just add the value
            u8 val = *current;
            symbolCounts[val]++;
            output.push_back(Symbol{val, 0, 0, false});
        }

        // Add the characters into the stream 
        for(int i = chars_to_add; i > 0; i--) {
            if (std::cin.get(next_byte)){
                bytes_read++;
                crc = CRC::Calculate(&next_byte,1, crc_table, crc); //Add the character we just read to the CRC (even though it is not in a block yet)
                buffer.push_back(next_byte);
            }
            if(buffer.size() > MAX_BACKREF_DIST){//avoid having the buffer be too large and avoid having backreferences that are too long
                std::string key {3, 'a'};
                auto bi = buffer.begin();
                for(auto it = key.begin(); it != key.end(); it++) {
                    *it = *bi;
                    bi++;
                }
                auto mi = m.find(key);
                (*mi).second.pop_back();
                if((*mi).second.empty()) {
                    m.erase(key);//remove this reference from the map, it can't be used anymore because it is too far back
                }
                buffer.pop_front(); //can't make back references longer than this
            }
            if(buffer.size() >= 6) {
                std::list<u8>::iterator x {current};
                std::string key {3, 'a'};
                //make a key of the first three chars of the input buffer
                for(auto it = key.begin(); it != key.end(); it++) {
                    *it = *x;
                    x++;
                }
                std::list<u8>::iterator y {current};
                auto li = m.find(key);
                //if the key exists, add an iterator to it
                //if not create a new list and add it
                if(li == m.end()) {
                    std::list<std::list<u8>::iterator> newlist {y};
                    m.insert({key, newlist});
                } else {
                    (*li).second.push_front(y);
                }
            }
            
            if (current == buffer.end()) break;
            current++;

        }

        if (current == buffer.end()) { 
            break; //nothing left
        } else if (output.size() > MAX_BLOCK_SIZE) {
            write_block(stream, output, false, 2); //at least one thing left and we have a full block
            output.clear();
            for(int x = 0; x < SS_TABLE_SIZE; x++) symbolCounts[x] = 0;
            for(int x = 0; x < DIST_TABLE_SIZE; x++) distCounts[x] = 0;
        }
    }


    //At this point, we've finished reading the input (no new characters remain), and we may have an incomplete block to write.
    if (output.size() > 0){
        if(output.size() < 200) { // not really worth it to write block type 2 for things less than 500 bytes in size
            write_block(stream,output,true, 1);
        } else {
            write_block(stream,output,true, 2);
        }
    }
    //After the last block, restore byte alignment
    stream.flush_to_byte();

    //Now close out the bitstream by writing the CRC and the total number of bytes stored.
    stream.push_u32(crc);
    stream.push_u32(bytes_read);

    return 0;
}