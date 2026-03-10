#include <kernel.h>
#include <sifrpc.h>
#include <stdio.h>
#include <malloc.h>

#include "runner.h"
#include "vm.h"
#include "../data_win.h"

int main(int argc, char* argv[]) {
    SifInitRpc(0);

    const char* dataWinPath = "host:data.win";
    if (argc > 1) {
        dataWinPath = argv[1];
    }

    printf("Butterscotch PS2 - Loading %s\n", dataWinPath);

    DataWin* dataWin = DataWin_parse(
        dataWinPath,
        (DataWinParserOptions) {
            .parseGen8 = true,
            .parseOptn = true,
            .parseLang = true,
            .parseExtn = true,
            .parseSond = true,
            .parseAgrp = true,
            .parseSprt = true,
            .parseBgnd = true,
            .parsePath = true,
            .parseScpt = true,
            .parseGlob = true,
            .parseShdr = true,
            .parseFont = true,
            .parseTmln = true,
            .parseObjt = true,
            .parseRoom = true,
            .parseTpag = true,
            .parseCode = true,
            .parseVari = true,
            .parseFunc = true,
            .parseStrg = true,
            .parseTxtr = false,
            .parseAudo = false
        }
    );

    // Initialize VM
    VMContext* vm = VM_create(dataWin);

    Runner* runner = Runner_create(dataWin, vm);

    // Initialize the first room and fire Game Start / Room Start events
    Runner_initFirstRoom(runner);

    printf("Starting loop...\n");
    while (!runner->shouldExit) {
        Runner_step(runner);
    }

    DataWin_free(dataWin);

    return 0;
}
