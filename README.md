Dana Wiltsie

Features Implemented: 
    I use an advanced data structure to find good back references. I maintain an unordered map where the keys are all unique three character sequences I see in the history, and the values are lists of iterators to the first letters of those sequences, with the most recent ones being closest to the beginning in the list. Then to find good back references I just check to see if the first three characters of the input buffer is a key, and if it is, go through the list of iterators to find an encoding that is good enough.

    I optimize the block 2 header, using the RLE features and also creating an optimal prefix code for the CLs. I then used the block type 2 header features to only encode the non-zero symbols at the end of the LL, the Distance, and the CL tables.

High-level Description:
    To hold the history and input data I use a list.

           current pointer
                ^
    [history... | look ahead]

    The look ahead always contains 286 characters, and the list in total only holds a max of 32768 characters. Until this limit is reached, as we process the input, we increment the current pointer, and then appropriately add new characters to the end of the list to maintain the look ahead. When we have 32768 characters, we also remove from the beginning of the list for each character added. 

    To speed up finding backreferences I also maintain an unordered map where the keys are all unique three character sequences I see in the history, and the values are lists of iterators to the first letters of those sequences, with the most recent ones being closest to the beginning in the list. Then to find good back references I just check to see if the first three characters of the input buffer is a key, and if it is, go through the list of iterators to find an encoding that is good enough.

    for example, the input buffer could be a,b,c,d,e,f... so I could lookup "abc" in the map to see if we have seen that before. If so, we get returned a list of iterators into the history list that point us to the beginning of those sequences. Say we get pointed to a place in history that contains a,b,c,d,t... Then we would compare the characters one by one until we see that they don't match. In this case, we would get a length of 4. If we have decided that 4 is good enough (there is a threshold) then unfortunatly we have to go forward through the list until we reach the end of the history to count the distance, but this is a small price to pay for finding backreferences quickly.

    Then we start printing blocks. For almost all cases we use block type 2. It is pretty standard, I adapted a huffman code algorithm from https://www.geeksforgeeks.org/huffman-coding-greedy-algo-3/ to give me the correct lengths. I used an algorithm similar to yours (Bill Bird) to enforce the max length. Everything past this point is pretty standard besides the way I optimize the CL code. I had to do a strange nested while loop that is probably more complicated than it needed to be, but basically we keep a count of how many of the same character we've seen in a row, and then when we see a different character, we handle the output of the previous one. 