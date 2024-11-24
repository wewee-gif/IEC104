#include <stdio.h>
#include <uv.h>
#include "converter.h"

uv_loop_t *loop;
uv_fs_t stdin_watcher;
char buffer[1024];

uv_timer_t job_req;

void job(){
    
    printf("11111111111\n");
    
}

void on_type(uv_fs_t *req) {
    if (stdin_watcher.result > 0) {
        buffer[stdin_watcher.result] = '\0';
        printf("Typed %s\n", buffer);
        trim_whitespace_simple(buffer);
        printf("after:%s\n",buffer);
        // if(buffer[0]=='Y'){
        //     printf("22222\n");
        //     if (uv_timer_again(&job_req) !=-1){
        //         printf("ÖØÖÃ\n");
        //     }
        // }
        
    }
    else if (stdin_watcher.result < 0) {
        fprintf(stderr, "error opening file: %s\n", uv_strerror(req->result));
    }
    uv_buf_t buf = uv_buf_init(buffer, 1024);
    uv_fs_read(loop, &stdin_watcher, 0, &buf, 1, -1, on_type);
}

int main(){
    loop = uv_default_loop();
    // uv_timer_init(loop, &job_req);
    // uv_timer_start(&job_req, job, 0, 10000);

    uv_buf_t buf = uv_buf_init(buffer, 1024);
    uv_fs_read(loop, &stdin_watcher, 0, &buf, 1, -1, on_type);

    return uv_run(loop, UV_RUN_DEFAULT);
}