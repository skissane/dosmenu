#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <dir.h>
#include <ctype.h>

#define true 1
#define false 0

#define LAUNCHER_VECTOR 0x88

void abortMsg(char *msg);

void * xmalloc(size_t size) {
        void *r = malloc(size);
        if (r == NULL)
                abortMsg("Memory allocation failed");
        return r;
}

char * xstrdup(char *str) {
	char *copy = xmalloc(strlen(str)+1);
	strcpy(copy,str);
	return copy;
}

char * xstrcat(char *a, char *b) {
	int na, nb;
	char *r;

	na = strlen(a);
	nb = strlen(b);
	r = xmalloc(na+nb+1);
	r[0] = 0;
	strcat(r, a);
	strcat(r, b);
	return r;
}

int fileExistenceCheck(char *fname) {
	FILE *fh = fopen(fname,"r");
	if (fh == NULL)
		return false;
	fclose(fh);
	return true;
}

int checkLauncherInstalled() {
	return getvect(LAUNCHER_VECTOR) != 0;
}

int callLauncher(char *exec, char *args, int pause) {
	union REGS in;
	memset(&in,0,sizeof(union REGS));
	in.x.ax = 0x1369;
	in.x.bx = (int)exec;
	in.x.cx = pause;
	in.x.si = (int)args;
	return int86(LAUNCHER_VECTOR, &in, &in);
}

// Split cmd into command line and argument
void launcherSystem(char *line, int pause) {
	char *comspec, *arg;
	int rc;

	comspec = getenv("COMSPEC");
	if (comspec == NULL || strlen(comspec) == 0)
		abortMsg("COMSPEC not set");
	if (!fileExistenceCheck(comspec))
		abortMsg("Command interpreter (COMSPEC) not found");

	arg = xstrcat("/C ",line);
	if (strlen(arg) > 126) {
		free(arg);
		abortMsg("Command line too long");
	}

	rc = callLauncher(comspec,arg,pause);
	free(arg);
	if (rc != 0)
		abortMsg("Launcher invocation failed");
	exit(0);
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
        s->name = xstrdup(name);
        s->settings = NULL;
        s->next = NULL;
        return s;
}

struct setting * newSetting(char *name, char* value) {
        struct setting * s = (struct setting*) xmalloc(sizeof(struct setting));
        s->name = xstrdup(name);
        s->value = xstrdup(value);
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

enum COLORS colorByName(char *name) {
	if (strcmp("BLACK",name) == 0) return BLACK;
	if (strcmp("BLUE",name) == 0) return BLUE;
	if (strcmp("GREEN",name) == 0) return GREEN;
	if (strcmp("CYAN",name) == 0) return CYAN;
	if (strcmp("RED",name) == 0) return RED;
	if (strcmp("MAGENTA",name) == 0) return MAGENTA;
	if (strcmp("BROWN",name) == 0) return BROWN;
	if (strcmp("LIGHTGRAY",name) == 0) return LIGHTGRAY;
	if (strcmp("DARKGRAY",name) == 0) return DARKGRAY;
	if (strcmp("LIGHTBLUE",name) == 0) return LIGHTBLUE;
	if (strcmp("LIGHTGREEN",name) == 0) return LIGHTGREEN;
	if (strcmp("LIGHTCYAN",name) == 0) return LIGHTCYAN;
	if (strcmp("LIGHTRED",name) == 0) return LIGHTRED;
	if (strcmp("LIGHTMAGENTA",name) == 0) return LIGHTMAGENTA;
	if (strcmp("YELLOW",name) == 0) return YELLOW;
	if (strcmp("WHITE",name) == 0) return WHITE;
	return -1;
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

int isCommentLine(char *line) {
	for (; *line != 0; line++) {
		if (*line == ';')
			return true;
		if (!isspace(*line))
			return false;
	}
	return false;
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
		// Skip comment lines
		if (isCommentLine(line)) {
			free(line);
			continue;
		}
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

void drawScreen(struct section *sections, char *title, int selected, int borderColor, int horizGap) {
        int i;
        char attrByte;
        clrscr();
        gotoxy(1 + horizGap,1);
        textbackground(BLACK);
        textcolor(borderColor);
        gotoxy(1 + horizGap,1);
        putch(201);
        for (i = 1 + horizGap; i < 79 - horizGap; i++)
                putch(205);
        putch(187);
        gotoxy(1 + horizGap,2);
        putch(186);
        gotoxy(80 - horizGap,2);
        putch(186);
        gotoxy(1 + horizGap,3);
        putch(204);
        for (i = 1 + horizGap; i < 79 - horizGap; i++)
                putch(205);
        putch(185);
        for (i = 4; i < 25; i++) {
                gotoxy(1+horizGap,i);
                putch(186);
                gotoxy(80-horizGap,i);
                putch(186);
        }
        gotoxy(1+horizGap,25);
        putch(200);
        for (i = 1+horizGap; i < 79-horizGap; i++)
                putch(205);

	if (horizGap == 0) {
		attrByte = peekb(0xB800, 1);
		pokeb(0xB800, ((25*80)-1)*2, 188);
		pokeb(0xB800, (((25*80)-1)*2)+1, attrByte);
	}
	else {
		gotoxy(80-horizGap,25);
		putch(188);
	}

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

// Check if a key is pending.
// Key is pending if head and tail pointers of keyboard buffer in
// BIOS data area (BDA) are different.
int keyPending() {
	int head, tail;
	disable();
	head = peek(0x40,0x1A); // 40:1A = keybuf head pointer
	tail = peek(0x40,0x1C); // 40:1C = keybuf tail pointer
	enable();
	return head != tail;
}

// Ignore all pending keys
// This helps consume any stray keys after exiting
void drainInput() {
	while (keyPending())
		readScanCode();
}

int main(int argc, char**argv) {
        struct section *sections;
        char *title, *saveCWD, *borderColorName, *strHBorder;
        int scanCode, inputLoop,selected, actionEnter, sectionCount, borderColor;
	int horizGap=0;

        saveCWD = xmalloc(256);
        getcwd(saveCWD, 256);

	// The launcher is an assembly language program used to reduce
	// memory consumption. If we find it run it instead of ourselves.
	if (fileExistenceCheck("DOSMENU.COM") &&
		!checkLauncherInstalled() &&
		fileExistenceCheck("LAUNCHER.COM")) {
			execl("LAUNCHER.COM","LAUNCHER.COM",NULL);
			// If we get to here, launcher failed
	}

	drainInput();
        resetVideo();
        _setcursortype(_NOCURSOR);
        sections = loadConfig();
        sectionCount = countSections(sections);

        title = getSetting(sections, "", "title");
        if (title == NULL)
                abortMsg("Setting 'title' not set");

	strHBorder = getSetting(sections, "", "horizgap");
	if (strHBorder != NULL)
		horizGap = atoi(strHBorder);
	if (horizGap < 0)
		abortMsg("Setting 'horizgap' has invalid value");

	borderColor = LIGHTGREEN;
	borderColorName = getSetting(sections, "", "border");
	if (borderColorName != NULL) {
		borderColor = colorByName(borderColorName);
		if (borderColor < 0)
			abortMsg("Setting 'border' has invalid value");
	}

        while (true) {
                actionEnter = false;
                selected = 1;
                inputLoop = true;
                drawScreen(sections, title, selected, borderColor, horizGap);
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
			char *pause = getSettingInSection(nth, "pause");

                        if (dir != NULL)
                                chdir(dir);
			if (checkLauncherInstalled())
				launcherSystem(run, strcmp(pause,"1")==0);
			else
				system(run);
			if (strcmp(pause,"1")==0) {
				drainInput();
				printf("Press any key to continue...");
				readScanCode();
				printf("\n");
			}
                        chdir(saveCWD);
                        resetVideo();
                        _setcursortype(_NOCURSOR);
			drainInput();
                }
        }
}
