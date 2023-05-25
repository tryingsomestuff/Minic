#include "lib/nnue_training_data_formats.h"

int main(int argc, char ** argv){

    if ( argc < 4 ) return 1;

    const std::string option = argv[1];
    const std::string input  = argv[2];
    const std::string output = argv[3];
    
    if (option == "binpack2bin"){
        binpack::convertBinpackToBin(input,output,std::ios::trunc,false);
    }
    else if (option == "binpack2plain"){
        binpack::convertBinpackToPlain(input,output,std::ios::trunc,false);
    }
    else if (option == "bin2binpack"){
        binpack::convertBinToBinpack(input,output,std::ios::trunc,false);
    }
    else if (option == "bin2plain"){
        binpack::convertBinToPlain(input,output,std::ios::trunc,false);
    }
    else if (option == "plain2bin"){
        binpack::convertPlainToBin(input,output,std::ios::trunc,false);
    }
    else if (option == "plain2binpack"){
        binpack::convertPlainToBinpack(input,output,std::ios::trunc,false);
    }
    else if (option == "count"){
        binpack::countCat(input);
    }

    return 0;
}

