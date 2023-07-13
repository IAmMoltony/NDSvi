#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <string>
#include <sstream>
#include <vector>
#include <filesystem.h>

typedef enum
{
    EditorMode_Normal,
    EditorMode_Insert,
} EditorMode;

void waitButton(void)
{
    while (true)
    {
        scanKeys();
        if (keysDown() && !(keysDown() & KEY_TOUCH))
            break;
        swiWaitForVBlank();
    }
}

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
    printf("ok\nInitializing NitroFS... ");
    if (!nitroFSInit(NULL))
    {
        printf("failed!\n\nThis shouldn't happen. If this happens, then it's a bug.");
        while (true)
            ;
    }

    std::string version;
    {
        FILE *fp = fopen("nitro:/NDSviVersion.txt", "r");
        if (!fp)
        {
            perror("Error opening NDSviVersion.txt");
            while (true)
                ;
        }
        int c;
        while ((c = getc(fp)) != EOF)
            version += c;
    }

    std::string document;
    std::string fileName;
    int cursorPos = 0;
    u16 frames = 0;
    bool cursorBlink = false;
    int lastCh = 0;
    EditorMode editMode = EditorMode_Normal;
    bool commandEnter = false;

    while (true)
    {
        consoleClear();

        if (frames % 30 == 0)
            cursorBlink = !cursorBlink;

        int ch = keyboardUpdate();
        switch (editMode)
        {
        case EditorMode_Normal:
            if (ch != -1)
                lastCh = ch;
            if (ch)
            {
                // key pressed

                switch (tolower(ch))
                {
                case 'i':
                    editMode = EditorMode_Insert;
                    break;
                case ':':
                    commandEnter = true;
                    break;
                }
            }
            break;
        case EditorMode_Insert:
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
                    if ((size_t)(cursorPos + 1) <= document.size())
                        ++cursorPos;
                    break;
                case -23: // escape thing
                    editMode = EditorMode_Normal;
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
        case EditorMode_Normal:
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
                if (command == "w" || command == "write")
                {
                    if (fileName == "")
                    {
                        // file name prompt
                        printf("File name: ");
                        while (true)
                        {
                            int ch = keyboardUpdate();

                            if (ch == -23)
                                goto gameLoopEnd;
                            if (ch == -1)
                                continue;
                            if (ch == '\n')
                                break;

                            if (ch > 0)
                            {
                                fileName += ch;
                                putchar(ch);
                            }
                        }
                    }
                    putchar('\n');

                    FILE *fp = fopen(std::string("fat:/" + fileName).c_str(), "w");
                    if (!fp)
                    {
                        perror(std::string("Error saving " + fileName).c_str());
                        waitButton();
                    }
                    else
                    {
                        fwrite(document.c_str(), sizeof(char), document.size(), fp); // TODO add error-checking
                        fclose(fp);
                        printf("Saved. Press a button to continue.");
                        waitButton();
                    }
                }
                else if (command.rfind("open", 0) == 0 || command.rfind("e", 0) == 0)
                {
                    // extract file name
                    std::string openFileName;
                    if (command.rfind("e", 0) == 0)
                        openFileName = command.substr(1);
                    else
                        openFileName = command.substr(4);

                    // check for no file name
                    if (openFileName.empty())
                    {
                        printf("No file name. Press a button to continue.");
                        waitButton();
                    }

                    openFileName.erase(0, 1); // remove space

                    // open file
                    FILE *fp = fopen(std::string("fat:/" + openFileName).c_str(), "r");
                    if (fp)
                    {
                        document = "";
                        int c;
                        while ((c = getc(fp)) != EOF)
                            document += c;
                    }
                    else
                    {
                        perror(std::string("Error opening " + openFileName).c_str());
                        waitButton();
                    }
                }
                else if (command == "version")
                {
                    consoleClear();
                    printf("NDSvi - Nintendo DS Vi\nVersion %s (compiled %s %s)\n\nPress a button to continue.",
                           version.c_str(), __DATE__, __TIME__);
                    waitButton();
                }
                else if (command != "")
                {
                    printf("Invalid command. Press a button to continue.");
                    waitButton();
                }
            }
            break;
        case EditorMode_Insert:
            printf("-- INSERT --");
            break;
        }

        commandEnter = false;

    gameLoopEnd:
        ++frames;
        swiWaitForVBlank();
    }

    return 0;
}
