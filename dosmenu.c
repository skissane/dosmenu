#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <dir.h>

#define true 1
#define false 0

void abortMsg(char *msg);

void * xmalloc(size_t size) {
        void *r = malloc(size);
        if (r == NULL)
                abortMsg("Memory allocation failed");
        return r;
}

void resetVideo(void) {
        textmode(C80);
        textcolor(LIGHTGRAY);
        textbackground(BLACK);
        _setcursortype(_NORMALCURSOR);
        clrscr();
}

void abortMsg(char *msg) {
        resetVideo();
        printf("ERROR: %s\n\n", msg);
        exit(1);
}

struct setting {
        char *name;
        char *value;
        struct setting *next;
};

struct section {
        char *name;
        struct setting *settings;
        struct section *next;
};

struct section * newSection(char *name) {
        struct section * s = (struct section*) xmalloc(sizeof(struct section));
        s->name = strdup(name);
        s->settings = NULL;
        s->next = NULL;
        return s;
}


struct setting * newSetting(char *name, char* value) {
        struct setting * s = (struct setting*) xmalloc(sizeof(struct setting));
        s->name = strdup(name);
        s->value = strdup(value);
        s->next = NULL;
        return s;
}

void addSetting(struct section *section, char* name, char* value) {
        struct setting *cur = section->settings;
        if (cur == NULL) {
                section->settings = newSetting(name,value);
                return;
        }
        while (cur->next != NULL)
                cur = cur->next;
        cur->next = newSetting(name,value);
}

#define LINE_MAX 256

char *readLine(FILE *fp) {
        char *buf;
        int ch, i = 0;
        buf = xmalloc(LINE_MAX);
        memset(buf, 0, LINE_MAX);
        while (true) {
                ch = fgetc(fp);
                if (ch == EOF || ch == '\n')
                        return buf;
                if (i < (LINE_MAX-1) && ch != '\r')
                        buf[i++] = ch;
        }
}

struct section *addSection(struct section *sections, char *name) {
        while (sections->next != NULL)
                sections = sections->next;
        return sections->next = newSection(name);
}

struct section *getSectionByIndex(struct section *sections, int index) {
        while (true) {
                if (sections == NULL || index < 0)
                        return NULL;
                if (index == 0)
                        return sections;
                sections = sections->next;
                index--;
        }
}

struct section *getSection(struct section *sections, char *name) {
        while (true) {
                if (sections == NULL)
                        return NULL;
                if (strcmp(sections->name,name) == 0)
                        return sections;
                sections = sections->next;
        }
}

char * getSettingInSection(struct section *section, char *name) {
        struct setting *setting;
        if (section == NULL)
                return NULL;
        setting = section->settings;
        while (true) {
                if (setting == NULL)
                        return NULL;
                if (strcmp(setting->name,name) == 0)
                        return setting->value;
                setting = setting->next;
        }
}

char *getSetting(struct section *sections, char *section, char*name) {
        sections = getSection(sections,section);
        if (sections == NULL)
                return NULL;
        return getSettingInSection(sections,name);
}

struct section *loadConfig(void) {
        FILE *cfg = NULL;
        struct section *sections = newSection("");
        struct section *curSection = sections;
        cfg = fopen("dosmenu.cfg","r");
        if (cfg == NULL)
                abortMsg("Opening dosMenu.cfg failed");

        while (true) {
                char *line = readLine(cfg);
                if (strlen(line) == 0) {
                        free(line);
                        break;
                }
                //printf("LINE:%s\n",line);
                if (strchr(line,'[') == line && strchr(line,']') != NULL) {
                        char *sect = line + 1;
                        char *endSect = strchr(line,']');
                        *endSect = 0;
                        curSection = addSection(sections,sect);
                }
                else {
                        char *kvs = strchr(line,'=');
                        char *val = kvs+1;
                        if (kvs == NULL) {
                                resetVideo();
                                printf("ERROR: Bad syntax in input line [%s]\n", line);
                                exit(1);
                        }
                        *kvs = 0;
                        addSetting(curSection, line, val);
                }
                free(line);
        }
        fclose(cfg);
        return sections;
}

void centerText(int line, char *msg) {
        int len = strlen(msg);
        int col = ((80 - len) / 2) + 1;
        gotoxy(col,line);
        cputs(msg);
}

int readScanCode() {
        union REGS in, out;
        in.h.ah = 0;
        int86(0x16, &in, &out);
        return out.h.ah;
}

#define UP 72
#define DOWN 80
#define ENTER 28
#define SPACE 57
#define ESC 1

void drawScreen(struct section *sections, char *title, int selected) {
        int i;
        char attrByte;
        clrscr();
        gotoxy(1,1);
        textbackground(BLACK);
        textcolor(LIGHTGREEN);
        gotoxy(1,1);
        putch(201);
        for (i = 1; i < 79; i++)
                putch(205);
        putch(187);
        gotoxy(1,2);
        putch(186);
        gotoxy(80,2);
        putch(186);
        gotoxy(1,3);
        putch(204);
        for (i = 1; i < 79; i++)
                putch(205);
        putch(185);
        for (i = 4; i < 25; i++) {
                gotoxy(1,i);
                putch(186);
                gotoxy(80,i);
                putch(186);
        }
        gotoxy(1,25);
        putch(200);
        for (i = 1; i < 79; i++)
                putch(205);

        attrByte = peekb(0xB800, 1);
        pokeb(0xB800, ((25*80)-1)*2, 188);
        pokeb(0xB800, (((25*80)-1)*2)+1, attrByte);

        textcolor(YELLOW);
        centerText(2, title);

        for (i = 1; i < 22; i++) {
                struct section * nth = getSectionByIndex(sections, i);
                if (nth == NULL)
                        break;
                textbackground(i==selected ? RED: BLACK);
                textcolor(YELLOW);
                centerText(3+i, nth->name);
        }
}

void redrawLine(struct section *sections, int line, int isSelected) {
        struct section * nth = getSectionByIndex(sections, line);
        if (nth == NULL)
                return;
        textbackground(isSelected ? RED: BLACK);
        textcolor(YELLOW);
        centerText(3+line, nth->name);
}

int countSections(struct section *sections) {
        int i = 0;
        while (sections != NULL) {
                i++;
                sections = sections->next;
        }
        return i;
}

int main(int argc, char**argv) {
        struct section *sections;
        char *title, *saveCWD;
        int scanCode, inputLoop,selected, actionEnter, sectionCount;

        saveCWD = xmalloc(256);
        getcwd(saveCWD, 256);

        resetVideo();
        _setcursortype(_NOCURSOR);
        sections = loadConfig();
        sectionCount = countSections(sections);

        title = getSetting(sections, "", "title");
        if (title == NULL)
                abortMsg("Setting 'title' not set");

        while (true) {
                actionEnter = false;
                selected = 1;
                inputLoop = true;
                drawScreen(sections, title, selected);
                while (inputLoop) {
                        scanCode = readScanCode();
                        switch (scanCode) {
                                case DOWN:
                                        redrawLine(sections,selected,false);
                                        if (selected < 21 && selected < (sectionCount-1))
                                                selected++;
                                        redrawLine(sections,selected,true);
                                        break;
                                case UP:
                                        if (selected > 1) {
                                                redrawLine(sections,selected,false);
                                                selected--;
                                                redrawLine(sections,selected,true);
                                        }
                                        break;
                                case SPACE:
                                case ENTER:
                                        actionEnter = true;
                                        inputLoop = false;
                                        break;
                                case ESC:
                                        resetVideo();
                                        return 0;
                                default:
                                        // Ignore unknown char
                                        break;
                         }
                }
                resetVideo();

                if (actionEnter) {
                        struct section * nth = getSectionByIndex(sections, selected);
                        char *dir = getSettingInSection(nth, "dir");
                        char *run = getSettingInSection(nth, "run");
                        if (dir != NULL)
                                chdir(dir);
                        system(run);
                        chdir(saveCWD);
                        resetVideo();
                        _setcursortype(_NOCURSOR);
                }
        }
}