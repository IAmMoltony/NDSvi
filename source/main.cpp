#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <string>
#include <sstream>

typedef enum
{
    editmodeNormal,
    editmodeInsert,
} EditorMode;

int main(int argc, char **argv)
{
    PrintConsole topScreen;

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);

    keyboardDemoInit();
    keyboardShow();

    consoleInit(&topScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleSelect(&topScreen);

    printf("Initializing FAT... ");
    if (!fatInitDefault())
    {
        printf("failed!\n\nMake sure the ROM is patched properly and the SD card is present.");
        while (true)
            ;
    }
    printf("ok\n");

    std::string document;
    int cursorPos = 0;
    u16 frames = 0;
    bool cursorBlink = false;
    int lastCh = 0;
    EditorMode editMode = editmodeNormal;
    bool commandEnter = false;

    while (true)
    {
        consoleClear();

        if (frames % 30 == 0)
            cursorBlink = !cursorBlink;

        int ch = keyboardUpdate();
        switch (editMode)
        {
        case editmodeNormal:
            if (ch != -1)
                lastCh = ch;
            if (ch)
            {
                // key pressed

                switch (tolower(ch))
                {
                case 'i':
                    editMode = editmodeInsert;
                    break;
                case ':':
                    commandEnter = true;
                    break;
                }
            }
            break;
        case editmodeInsert:
            if (ch != -1)
                lastCh = ch;
            if (ch)
            {
                // key pressed

                switch (ch)
                {
                case -20: // left arrow
                    if (cursorPos - 1 >= 0)
                        --cursorPos;
                    break;
                case -18: // right arrow
                    if (cursorPos + 1 <= document.size())
                        ++cursorPos;
                    break;
                case -23: // escape thing
                    editMode = editmodeNormal;
                    break;
                default:
                    if (ch > 0)
                    {
                        document.insert(cursorPos, 1, (char)ch);
                        ++cursorPos;
                    }
                    break;
                }
            }
            break;
        }

        // render the document
        std::string line;
        std::istringstream documentStream(document);
        int lines = 0;
        int i = 0;
        while (std::getline(documentStream, line))
        {
            for (auto ch : line)
            {
                if (i == cursorPos - 1 && cursorBlink)
                    putchar(' ');
                else
                    putchar(ch);

                ++i;
            }
            putchar('\n');
            ++lines;
        }
        i = 0;

        for (int i = lines + 3; i < topScreen.windowHeight; ++i)
        {
            printf("~\n");
        }
        printf("%d\n", lastCh);
        switch (editMode)
        {
        case editmodeNormal:
            if (commandEnter)
            {
                putchar(':');
                keyboardUpdate();
                std::string command;
                while (true)
                {
                    int ch = keyboardUpdate();
                    if (ch > 0)
                    {
                        switch (ch)
                        {
                        case '\n':
                            goto exitCommandEnter;
                            break;
                        case '\b':
                            if (command.size())
                            {
                                command.pop_back();
                                putchar('\b');
                            }
                            break;
                        default:
                            command += (char)ch;
                            putchar(ch);
                            break;
                        }
                    }
                }
            exitCommandEnter:
                putchar('\n');
                if (command == "w")
                {
                    printf("write to file here...... press a button");
                    while (true)
                    {
                        scanKeys();
                        if (keysDown() && !(keysDown() & KEY_TOUCH))
                            break;
                    }
                }
                else
                {
                    printf("Invalid command. Press a button to continue.");
                    while (true)
                    {
                        scanKeys();
                        if (keysDown())
                            break;
                    }
                }
            }
            break;
        case editmodeInsert:
            printf("Insert mode");
            break;
        }

        commandEnter = false;

        ++frames;
        swiWaitForVBlank();
    }

    return 0;
}
