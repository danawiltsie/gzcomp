# GZComp - A gzip/DEFLATE Compliant Compressor
A C++ program to compress any file in a format that gzip can decompress. 

## Table of contents
* [General info](#general-info)
* [GZComp overview](#gzcomp-overview)
* [Running GZComp](#running-gzcomp)

## General info
For those who are unaware, gzip is a single-stream lossless compression/decompression program that was released in 1993. It is based on the DEFALATE algorithm, an algorithm for file compression that is based upon LZ77 and Huffman coding. Compressed files produced by gzip have the ".gz" suffix. For more information about gzip and DEFALTE, please see the following materials that were used to create GZComp. The first is <a href="https://tools.ietf.org/html/rfc1951" target="_blank">RFC 1951</a> which is the specification for the DEFALTE algorithm. The second is <a href="https://tools.ietf.org/html/rfc1952" target="_blank">RFC 1952</a> which specifies the gzip wrapper around the DEFLATE algorithm. 

At a high level, the DEFALTE algorithm works to reduce redundancy in the input file by generating a back-reference in the place of any string of characters that it encounters that it has seen previously. These back-references include two numbers: how long the repeated string of characters is, and how far back in the file that the repeated string was encountered. The remaining text with backreferences is then encoded using a complicated series of huffman codes in order to further reducce file size. I heavily encourage everyone to look over RFC 1951 to get a better understanding how some of the finer details of the algorithm. 

## GZComp overview: 
 To hold the history and input data, GZComp uses a list.

         current pointer
                ^
    [history... | look ahead]

The look ahead always contains 286 characters (provided there are enough characters left in the input file to fill the buffer), and the list in total only holds a max of 32768 characters. Until this limit is reached, as we process the input, we increment the current pointer, and then appropriately add new characters to the end of the list to maintain the look ahead. When we have 32768 characters, we also remove from the beginning of the list for each character added. The limit placed on the number of characters in the history and the lookahead is specified by the DEFALTE algorithm. 

GZComp uses an advanced data structure to find good back references. The program maintains an unordered map where the keys are all unique three character sequences encountered in the history of the file, and the values are lists of iterators to the first letters of those sequences, with the most recent ones being closest to the beginning in the list. Then to find good back references the program checks to see if the first three characters of the input buffer is a key in the map, and if it is, go through the list of iterators to find an encoding that is good enough. 

What counts as "good enough" is specified by a theshold. There is a balance to be found between speed and compression performance here. A lower threshhold will make the program less picky, increasing speed. However, the backreferences generated will be less optimal. A really high threshold will make the program more picky with backreferences, greater reducing the size of the input at the cost of speed. However, since the program uses this optimized data structure, the speed at which it can find good backreferences is greatly improved over linearly searching through input for good backreferences. 

For example, the input buffer could be a,b,c,d,e,f... so the program would lookup "abc" in the map to see if it has seen that string before. If so, it finds a list of iterators into the history list that point us to the beginning of those sequences. Say the program get pointed to a place in history that contains a,b,c,d,t... Then it would compare the characters one by one until we see that they no longer match. In this case, we would get a length of 4. If we have decided that 4 is good enough (there is a threshold) then the program moves forward through the history until we reach the end in order to count the distance. This seems wasteful, but this is a small price to pay for finding backreferences quickly.

GZComp also optimizes the header for each block of compressed output using run-length-encoding and creating an optimal prefix code for the code lengths. I then used the block type 2 header features to only encode the non-zero symbols at the end of the literal, distance, and the code length tables. For more information about this optimization, please see section 3.2.7 of RFC 1951. 

## Running GZComp
To run the program, simply clone this repository, and from the root directory run `./validate` to make sure the program is working as intended. This bash script compiles the code and runs the compression algorithm on the large variety of files located in the test_data directory. An example line of output is: 

`Checking ./test_data/calgary_corpus/book1
Passed: 321427 bytes (239%)`

The compressed size in bytes is printed, as well as a percentage. This percentage indicates the amount of compression achived compared to the original file. The calculation is `(original_size / compressed_size)`. In the example, 239% means that the original file is 2.39 times bigger than the compressed output. 

To run GZComp on your own file, run `make` and then the command:
`./gzcomp < input_file > output_file` where `input_file` is the path to the file you want to compress, and `output_file` is the path and name of the resulting compressed file. Then to decompress, use gzip, the command:
`gzip -d < compressed_file > decompressed_file`


