#include <stdio.h>
#include <string.h>

int getChecksum(const char* string){
    
    int sum = 0;

    for(int i = 0; strlen(string); i++){
        sum += (int)string[i];
    }

    int checksum = (255 - (sum % 255));

    return checksum;
    /*
    1)Create a way to convert characters to ascii
    2)*/
}