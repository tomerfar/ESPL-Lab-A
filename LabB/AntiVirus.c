#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
typedef struct virus {
        unsigned short SigSize;
        char virusName[16];
        unsigned char* sig;

    } virus;

typedef struct link link;

struct link {
    link *nextVirus;
    virus *vir;
};

char sigFile[256] = "signatures.L";
char suspectetFileName[256] = "";
char buffer[1024];
bool isBigEndian = false;
link* virus_list = NULL;


//Functions declarations
void SetSigFileName();
virus* readVirus(FILE* file);
void printVirus(virus* v);
void list_print(link* virus_list, FILE* stream);
link* list_append(link* virus_list, virus* data);
void list_free(link* virus_list);

//Functions declarations


//Linked List methods
void list_print(link *virus_list, FILE* list_data){
    link *current = virus_list;
    while(current != NULL){
        printVirus(current->vir);
        current = current->nextVirus;
    }

}

link* list_append(link *virus_list, virus* data){
    link *new_head = malloc(sizeof(link));
    if(new_head == NULL){
        fprintf(stderr, "Failed to allocate memory for new link.\n");
        return virus_list;
    }
    new_head->vir = data;
    new_head->nextVirus = virus_list;

    return new_head;
}

void list_free(link *virus_list){
    link *current = virus_list;
    while(current != NULL){
        link *next = current->nextVirus;
        free(current);
        current = next;
    }
}
//Linked List methods

void SetSigFileName(){
    printf("Enter a new signature file name: ");
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;
        strcpy(sigFile, buffer);
    } else {
        printf("Error reading input. Please try again.\n");
    }
}

virus* readVirus(FILE* file){

    virus* vir = malloc(sizeof(virus)); // remember to free this memory afterwards
    if(vir == NULL){
        fprintf(stderr,"Error: could not allocate memory for virus structure.\n");
        return NULL;
    }
    if(fread(vir, 18, 1 , file) != 1){ /* when read data from file into a struct,
     the data is stored in the fields of the struct in the order in which is read from the file.*/
        fprintf(stderr, "Error in reading virus length and name\n");
        free(vir);
        return NULL;
    }
   
    if(isBigEndian){
        vir->SigSize = (vir->SigSize >> 8) | (vir->SigSize << 8);
    }

    vir->sig = (unsigned char*)malloc(vir->SigSize);
    if(vir->sig == NULL){
        fprintf(stderr, "Error: could not allocate memory for virus signature.\n");
        free(vir);
        return NULL;
    }
    if(fread(vir->sig,vir->SigSize,1,file) != 1){ 
        fprintf(stderr, "Error in reading virus signature bytes\n");
        free(vir->sig);
        free(vir);
        return NULL; 
    }

    return vir;
}

void printHex(unsigned char* buffer, int length){
    for(int i = 0; i < length; i++){
        printf("%02X ",buffer[i]);
    }
    printf("\n");
}

void printVirus(virus* virus){ //notice we changed here the signature to be a better fit for us at the moment.
    printf("Virus details:\n");
    printf("virus name: %s\n", virus->virusName);
    printf("virus signature length: %d\n", virus->SigSize);
    printf("virus signature: ");
    printHex(virus->sig, virus->SigSize);
    printf("\n");
}

void DetectViruses() {
    printf("DetectViruses function not implemented\n");
}

void FixFile() {
    printf("FixFile function not implemented\n");
}

void Quit() {
    printf("Quitting the program\n");
    list_free(virus_list);
    exit(0);
}


int main(int argc, char **argv)
{
     if(argc < 2){
        fprintf(stderr, "Error: no file name provided.\n");
        exit(1);
    }
    FILE *file = fopen(argv[1], "rb"); // opening file in reading bytes mode.
    if(file == NULL){
        fprintf(stderr, "Error: could not open file %s", argv[1]);
        exit(1);
    }
   
    while(1){ // infinite loop
        printf("Menu:\n");
        printf("0) Set signatures file name\n");
        printf("1) Load signatures\n");
        printf("2) Print signatures\n");
        printf("3) Detect viruses\n");
        printf("4) Fix file\n");
        printf("5) Quit\n");
        printf("Please select a function:\n");   

        char input[10];
        int option;
        if(fgets(input, sizeof(input), stdin) == NULL){ //EOF
            break;
        }
        if(sscanf(input, "%d", &option) != 1){
            option = -1; //symbol for invalid option
        }

        switch(option){
            case 0: {
                SetSigFileName();
                break;
            }
            case 1:{
                FILE* file = fopen(sigFile, "rb");
                if(file == NULL){
                    fprintf(stderr,"Error: could not open file %s\n", sigFile);
                    break;;
                }
                char magic_buffer[5]; // 4 bytes for magic number and 1 for null terminator
                if(fread(magic_buffer, 1, 4 , file) != 4){
                    fprintf(stderr, "Error: could not read magic number from file %s", sigFile);
                    fclose(file);
                    break;
                }
                magic_buffer[4] = '\0';
                if (memcmp(magic_buffer, "VIRL", 4) != 0 && memcmp(magic_buffer, "VIRB", 4) != 0) {
                    fprintf(stderr, "Error: incorrect magic number in file %s", sigFile);
                    fclose(file);
                    break;
                }
                else if(memcmp(magic_buffer, "VIRL", 4) == 0){
                    isBigEndian = false;
                }
                else if(memcmp(magic_buffer, "VIRB", 4) == 0){
                    isBigEndian = true;
                }
                virus* vir;
                while((vir = readVirus(file)) != NULL){
                    virus_list = list_append(virus_list,vir);
                }
                fclose(file);
                break; 
            }
            case 2:{
                list_print(virus_list, stdout);
                break;
            }


        }
   
    }

    return 0;



}