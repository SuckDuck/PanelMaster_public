#include <stdbool.h>
#include "origami.h"

extern void Window0();

int main(int argc, char **argv){
    
    Window0();
    
    OG_Init("origami_example", 60);
    while (true) {
        if (OG_Update()){
            break;
        }
    }

    OG_Free();
    return 0;
}