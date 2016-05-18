/*
 * CS354: Operating Systems.
 * Purdue University
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_BUFFER_LINE 2048

// Buffer where line is stored
int line_length;
int line_index;
char line_buffer[MAX_BUFFER_LINE];

// Simple history array
// This history does not change.
// Yours have to be updated.
int history_index = 0;
char ** history;
int history_length = 0;

void read_line_print_usage()
{
    char * usage =
    "\n"
    " Usage(ctrl-?)        Print usage\n"
    " Backspace(ctrl-H)    Deletes last character\n"
    " up arrow             See the previous command in the history\n"
    " Down arrow           Shows the next command in the history list."
    " left arrow           key: Move the cursor to the left and allow insertion at that position\n"
    " right arrow          key: Move the cursor to the right and allow insertion at that position\n"
    " delete               key(ctrl-D): Removes the character at the cursor\n"
    " Home                 key (or ctrl-A): The cursor moves to the beginning of the line\n"
    " End                  key (or ctrl-E): The cursor moves to the end of the line\n";
    
    write(1, usage, strlen(usage));
}

/*
 * Input a line with some basic editing.
 */
char * read_line() {
    struct termios orig_attr;
    tcgetattr(0, &orig_attr);
    // Set terminal in raw mode
    tty_raw_mode();
    line_index = 0;
    line_length = 0;
    if (history == NULL) {  // whiout the check, up-arrow will give seg fault
        history = (char **)malloc(100 * sizeof(char *));
    }
    // Read one line until enter is typed
    while (1) {
        // Read one character in raw mode.
        char ch;
        read(0, &ch, 1);
        
        if (ch>=32 && ch != 127) {
            // a printable character.
            
            int i = line_length;
            while (i > line_index) {
                line_buffer[i] = line_buffer[i - 1];
                i--;
            }
            line_buffer[i] = ch;
            line_length++;
            // If max number of character reached return.
            if (line_length==MAX_BUFFER_LINE-2) break;


            i = line_index;
            while ( i < line_length) {
                ch = line_buffer[i];
                write(1, &ch, 1);
                i++;
            }
            
            ch = 8;
            i = line_length;
            while ( i > line_index + 1) {
                write(1, &ch, 1);
                i--;
            }
            
            line_index++;
        }
        else if (ch==10) {
            // <Enter> was typed. Return line
            write(1, "\n", strlen("\n"));
            line_buffer[line_length] = 0;
            if (line_buffer[0] != 0) {
                history[history_length] = strdup(line_buffer);
                history_length++;
                history_index++;
            } else {
                write(1, "myshell>", strlen("myshell>"));
            }
            break;
        }
        else if (ch == 31) {
            // ctrl-?
            read_line_print_usage();
            line_buffer[0]=0;
            write(1, "myshell>", strlen("myshell>"));
            break;
        }
        else if (ch == 8 || ch == 127) {
            // <backspace> was typed. Remove previous character read.
            if (line_length == 0 || line_index == 0){
                continue;
            } else {
                // Go back one character
                int i = line_index - 1;
                while(i < line_length - 1){
                    line_buffer[i] = line_buffer[i+1];
                    i++;
                }
                ch = 8;
                write(1,&ch,1);

                i = line_index;
                while(i < line_length){
                    ch = line_buffer[i-1];
                    write(1,&ch,1);
                    i++;
                }
            
                // Write a space
                ch = ' ';
                write(1,&ch,1);
            
                // Go back one character
                ch = 8;
                write(1,&ch,1);
                i = line_index;
                while(i < line_length){
                    write(1,&ch,1);
                    i++;
                }
                // Remove one character from buffer
                line_index--;
                line_length--;
            }
            
        }
        else if (ch==27) {
            // Escape sequence. Read two chars more
            //
            // HINT: Use the program "keyboard-example" to
            // see the ascii code for the different chars typed.
            //
            char ch1;
            char ch2;
            read(0, &ch1, 1);
            read(0, &ch2, 1);
            // Up arrow. Print previous line in history.
            if (ch1 == 91 && ch2 == 65 && history_length > 0) {
                // Erase old line
                // Print backspaces
                int i = 0;
                while (i < line_length) {
                    ch = 8;
                    write(1, &ch, 1);
                    i++;
                }
                
                // Print spaces on top
                i = 0;
                while ( i < line_length) {
                    ch = ' ';
                    write(1, &ch, 1);
                    i++; 
                }
                
                // Print backspaces
                i = 0;
                while ( i < line_length) {
                    ch = 8;
                    write(1, &ch, 1);
                    i++;
                }
                
                // Copy line from history
                history_index = (history_index - 1);
                if (history_index == -1) {
                    history_index = 0;
                } else {
                    strcpy(line_buffer, history[history_index]);
                }
                
                line_length = strlen(line_buffer);
                // echo line
                write(1, line_buffer, line_length);
                line_index = line_length;
            }
            // down arrow, similar to UP but print next line in history
            if (ch1 == 91 && ch2 == 66) {
                // down arrow. Print next line in history.
                
                // Erase old line
                // Print backspaces
                int i = line_length - line_index;
                while ( i < line_length) {
                    ch = 8;
                    write(1, &ch, 1);
                    i++;
                }
                
                // Print spaces on top
                i = 0;
                while ( i < line_length) {
                    ch = ' ';
                    write(1, &ch, 1);
                     i++;
                }
                
                // Print backspaces
                i = 0; 
                while (i < line_length) {
                    ch = 8;
                    write(1, &ch, 1);
                    i++;
                }
                
                if (history_index >= 0 && history_length != 0) {
                    //printf("history_index > 0 before history[%d]:%s\n",history_index,history[history_index]);
                    history_index ++;
                    if (history_index >= history_length) {
                        history_index = history_length;
                        strcpy(line_buffer, "");
                    } else {
                        strcpy(line_buffer, history[history_index]);
                    }
                    //printf("history_index > 0 after history[%d]:%s\n",history_index,history[history_index]);
                } else {
                    // Copy line from history
                    strcpy(line_buffer, "");
                }
                //printf("history_index !> 0 after history[%d]:%s\n",history_index,history[history_index]);
                
                line_length = strlen(line_buffer);

                // echo line
                write(1, line_buffer, line_length);
                line_index = line_length;
            }
            // left arrow
            if (ch1 == 91 && ch2 == 68) {
                if (line_index > 0) {
                    ch = 8;
                    write(1, &ch, 1);
                    line_index--;
                }
            }
            
            // right arrow
            if (ch1 == 91 && ch2 == 67) {
                if (line_index < line_length) {
                    // char ch = line_buffer[line_index];
                    // write(1, &ch, 1);
                    write(1, &ch, 1);
                    write(1, &ch1, 1);
                    write(1, &ch2, 1);
                    line_index++;
                }
            }
        } else if (ch == 1) {
            // Home
            ch = 8;
            while(line_index > 0){
                write(1,&ch,1);
                line_index--;
            }
        } else if (ch == 5) {
            //End
            while (line_index < line_length + 1) {
                ch = line_buffer[line_index - 1];
                write(1, &ch, 1);
                line_index++;
            }
            line_index--;
        } else if (ch == 4) {
            //Delete
            if (line_length == 0 || line_index == line_length) {
                continue;
            } else {
                int i = line_index;
                
                while(i < line_length - 1) {
                    line_buffer[i] = line_buffer[i + 1];
                     i++;
                }
                
                line_length--;
                i = line_index;
                while( i < line_length) {
                    ch = line_buffer[i];
                    write(1, &ch, 1);
                    i++;
                }
                
                ch = ' ';
                write(1, &ch, 1);
                ch = 8;
                write(1, &ch, 1);
                i = line_index;
                while (i < line_length) {
                    write(1, &ch, 1);
                    i++;
                }
            }
        } else if (ch == 9) {
            //Tab. Path Completion
            //TODO
            
        }
        
    }
    // Add eol and null char at the end of string
    line_buffer[line_length]=10;
    line_length++;
    line_buffer[line_length]=0;


    tcsetattr(0, TCSANOW, &orig_attr);
    return line_buffer;
}

