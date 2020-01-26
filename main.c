#include "common.h"

int main(int argc, char * argv[]){
    if(argc == 3){
        client_thread(argc, argv);
    }else{
        server_thread(argc, argv);
    }
}