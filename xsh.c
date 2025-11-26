#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>

#define MAXLINE 1024
#define MAXARGS 64



// 문자열을 공백 기준으로 나누어 argv 배열 생성
int parse_line(char *line, char **argv) {
    int i = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL && i < MAXARGS - 1) {
        argv[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[i] = NULL; // 왜 널로 두지 -> 문자열 마지막
    return i;
}

// 반드시 함수는 선언 이전에 있어야함
// strrstr 함수 구현 (print_terminal에서 사용)
char *strrstr(const char *haystack, const char *needle) {
    char *result = NULL;
    char *p = strstr(haystack, needle);
    while (p != NULL) {
        result = p;
        p = strstr(p + 1, needle);
    }
    return result;
}

//터미널에 id와 현재 디렉토리를 포함하여 색깔 입혀 출력
void print_terminal()
{
    int userpid = getpid();  // user의 id 저장
    char cwd[MAXLINE];
    getcwd(cwd, sizeof(cwd)); // 현재 작업 디렉토리 가져오기
    
    // 마지막 dir만 출력
    char *temp = strrstr(cwd, "/");
    if (temp != NULL) {         // /가 있으면
        char new_cwd[MAXLINE];
        strcpy(new_cwd, temp+1);
        if (strcmp(new_cwd, "user") == 0)
        {
            strcpy(cwd, "~");
        }
        else
        {
            strcpy(cwd, new_cwd);
        }
    }
    printf("\033[1;33mXsh[%d]:\033[1;34m%s\033[0m> ", userpid, cwd); // 노란색으로 출력

}

// 환경변수 변경
// 중요! : 한 argv[n] 안에 여러 $~~가 존재할 수 있음.
// 따라서 한 변수에 모든 $을 탐색할 필요
// value가 없으면 \n만 출력
int is_memorized = 0; // argv 메모리 할당 여부 확인용 전역변수
void change_environ(char** argv)
{
    int i = 1;
    int k = 0;
    char *before_token;
    char environ[MAXARGS][MAXLINE];     // 환경변수 저장
    memset(environ,0,sizeof(environ));
    char *end;

    while (argv[i] != NULL)
    {
        // 임시 복사본 생성
        char temp_argv[strlen(argv[i])+1]; // 임시 복사용
        strncpy(temp_argv, argv[i], strlen(argv[i]));
        temp_argv[strlen(argv[i])] = '\0'; // 안전을 위해 널 종료
        before_token = temp_argv; // 복사본을 기준으로 작업

        // 토큰화($ 찾기) 시작
        //printf("before_token: %s\n",before_token);
        char *token = strtok(temp_argv,"$");     // token은 $ 이후의 값부터
        if (token == before_token)  // $가 없으면 복사만
        {
            strcpy(environ[i],argv[i]);
            //printf("environ = %s\n",environ[i]);
            i++;
            continue;
        }
        //printf("token: %s %ld\n",token,token-before_token);
        //environ[i] "$" 이전 문자열 복사
        // FIXME: token - before_token - 1 근데 이거 오버플로우 아닌가? -1되면 어떻게 되지
        int length = token - before_token - 1;
        strncpy(environ[i], before_token, length);
        environ[i][length] = '\0'; // 널 종료

        // 토큰이 NULL이 될 때 까지 계속해서 탐색
        while (token != NULL)
        {   
            //$가 존재하면 환경변수 탐색 시작
            end = strpbrk(token,"$ -();.\\\0<>&"); // $나 공백, 개행, 탭,NULL이 나오기 전까지 탐색
            // ★★ 중요! : end가 NULL이면 끝까지 탐색해야함 -> 안그러면 segmentation fault
            if (end == NULL)
            {
                end = token + strlen(token); // 끝까지 탐색
            }
            int length = end - token;
            //printf("end = %s",end);

            char *temp = (char*)malloc(length + 1);
            strncpy(temp, token, length);
            temp[length] = '\0';        // 환경변수 저장용
            //printf("env temp = %s\n",temp);

            // 환경변수 값 찾기
            char *value = getenv(temp);
            if (value != NULL)
            {
                strncat(environ[i], value, strlen(value));
            }
            // 환경변수 값이 없으면 아무것도 출력 안함
            before_token = token;  // $~~ 모두 제거
            token = strtok(NULL, "$");
            free(temp);

            // environ[i]에 중간 문자열 복사
            if (token != NULL)
            {
                int middle_length = token - end - 1 < 0 ? 0 : token-end -1; // -1은 $ 제거
                strncat(environ[i], end, middle_length);
                //printf("중간 문자열 복사 후 environ[%d] = %s\n",i,environ[i]);
            }
            else if (end != NULL)
            {
                // 마지막 문자열 복사
                strncat(environ[i], end, strlen(end));
                //printf("마지막 문자열 복사 후 environ[%d] = %s\n",i,environ[i]);
            }
            
            // 이건 cat할 필요가 없음
            // // environ[i]에 나머지 문자열 복사
            // if (token != NULL)
            // {
            //     strncat(environ[i], before_token, token - before_token);
            // }
            
        }
        // 마지막에 NULL 붙힐 필요 없음
        //printf("environ = %s\n",environ[i]);
        i++;
    }

    // 환경변수가 변환된 argv들 복사
    for (int j = 1; j < i; j++)
    {
        is_memorized = 1;
        argv[j] = (char*)malloc(strlen(environ[j]) + 1);
        strcpy((char*)argv[j], environ[j]);
        //printf("\033[1;32mchanged argv[%d]: %s\033[0m\n",j,argv[j]);
    }
}



int main(void) {
    char line[MAXLINE];
    char *argv[MAXARGS];
    pid_t pid;
    int status;
    int argc = 0;
    int is_background = 0;
    int k = 0; // 반복문 용 변수

    // 해당 시그널을 무시
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    while (1) {
        print_terminal();
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break; // EOF (Ctrl+D)
        }
        if (line[0] == '\n') continue;

        argc = parse_line(line, argv);
        if (argv[0] == NULL) continue;
        
        // TODO: 백그라운드 실행 확인
        if (strcmp(argv[argc - 1], "&") == 0) {
            is_background = 1;
            argv[argc - 1] = NULL; // & 제거
        } else {
            is_background = 0;
        }

        // TODO: 리다이렉션(>,<) 구현
        int is_stdin_redirect = 0, is_stdout_redirect = 0;
        k = 0;
        while (argv[k] != NULL) {
            if (strcmp(argv[k], ">") == 0) {
                is_stdout_redirect = 1;
                argv[k] = NULL; // > 이후는 명령어에서 제거
                break;
            } else if (strcmp(argv[k], "<") == 0) {
                is_stdin_redirect = 1;
                argv[k] = NULL; // < 이후는 명령어에서 제거
                break;
            }
            k++;
        }



        // 내장 명령어 처리
        if (strcmp(argv[0], "exit") == 0) break;
        if (strcmp(argv[0], "cd") == 0) {
            if (argv[1] == NULL) {
                fprintf(stderr, "cd: missing argument\n");
            } else if (chdir(argv[1]) != 0) {
                perror("cd");
            }
            continue;
        }
        //TODO: pwd 명령어 구현
        if (strcmp(argv[0], "pwd") == 0) {
            char cwd[MAXLINE];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("pwd");
            }
            continue;
        }

        // FIXME: fork에서 echo $VAR 처리 안됨 -> 무슨소리냐? : pid == 0가 작동을 안함

        // fork → exec → wait
        pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        } else if (pid == 0) {
            //TODO: 환경변수 예외처리
            change_environ(argv);
            //printf("여기 보세요~ : %s %s\n",argv[1],argv[2]);

            // --- [자식: 새로운 그룹 leader] ---
            // ★★ 자식은 백그라운드 여부 관계 없이 그룹leader가 됨
            setpgid(0, 0);   // 자식도 자기 그룹 leader
            // foreground 지정은 부모가 해줌

            // TODO: 리다이렉션 구현
            if (is_stdin_redirect) {
                // 표준 입력 리다이렉션
                FILE *input_file = fopen(argv[k + 1], "r");
                if (input_file == NULL) {
                    perror("fopen for input");
                    exit(1);
                }
                dup2(fileno(input_file), STDIN_FILENO);
                fclose(input_file);
            }
            if (is_stdout_redirect) {
                // 표준 출력 리다이렉션
                FILE *output_file = fopen(argv[k + 1], "w");
                if (output_file == NULL) {
                    perror("fopen for output");
                    exit(1);
                }
                dup2(fileno(output_file), STDOUT_FILENO);
                fclose(output_file);
            }


            // 명령어 실행
            execvp(argv[0], argv);
            printf("xsh: command not found: %s\n",argv[0]);
            exit(127);
        } else {
            // --- [부모: 자식 그룹 제어] ---
            if (is_background == 0)   // 포그라운드 실행일 때만
            {
                setpgid(pid, pid);                   // 자식을 그룹 leader로 확정
                tcsetpgrp(STDIN_FILENO, pid);        // 자식을 foreground로
            }

            // --- [부모: 자식 종료 대기] ---
            // TODO: argv[argc-1]가 &인지 확인 : 백그라운드 실행인지
            if (is_background)
            {
                printf("\033[1;33m[bg] started pid %d\033[0m\n", pid);
            }
            else
            {
                if (waitpid(pid, &status, WUNTRACED) < 0) {
                    perror("waitpid");
                }

                // --- [부모: 다시 셸로 제어 회수] ---
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }
        }
    }  // while 끝( 명령어 입력 루프 )

    // 환경변수 과정에서 할당한 argv 메모리 해제
    int final = 1;
    if (is_memorized == 0) return 0;   // 메모리 할당이 안되었으면 종료
    while(argv[final] != NULL)
    {
        free(argv[final]);
        final++;
    }
    return 0;
}
