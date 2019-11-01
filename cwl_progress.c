#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "cJSON.h"
#include "sds.h"

#define SERVER "https://workflows.service.oncopmnet.gr"


struct response {
    sds data;
};

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    // fprintf(stderr, "Got %zu members of %zu bytes each\n", nmemb, size);
    struct response* r = userdata;
    r->data = sdscatlen(r->data, ptr, size*nmemb);
    return nmemb;
}


cJSON* getProgress(const char* run_id)
{
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        return NULL;
    }
    sds url = sdsempty();
    url = sdscatprintf(url, "%s/api/workflows/v1/%s/metadata", SERVER, run_id);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    struct response r;
    r.data = sdsempty();
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    sdsfree(url);

    cJSON* json = cJSON_Parse(r.data);
    sdsfree(r.data);
    return json;
}

time_t str_to_time(const char* str)
{
    struct tm tm;
    strptime(str, "%FT%T", &tm);
    time_t t = mktime(&tm);
    return t;
}

double time_delta(const char* start, const char* end)
{
    time_t t1 = str_to_time(start);
    time_t t2 = str_to_time(end);
    double seconds = difftime(t2,t1);
    return seconds;
}

/* Some utils for reordering double lists */
void remove_node(cJSON** head, cJSON* node)
{
    assert(node);
    /* Removing first */
    if (*head == node) {
        *head = node->next;
    }

    if (node->next != NULL)
        node->next->prev = node->prev;
    if (node->prev != NULL)
        node->prev->next = node->next;
    node->next = node->prev = NULL;
}

void add_node_after(cJSON* node, cJSON* where)
{
    assert(where);
    assert(node);
    node->next = where->next;
    node->prev = where;

    if (where->next != NULL)
        where->next->prev = node;
    where->next = node;
}

void move_node_after(cJSON** head, cJSON* node, cJSON* where)
{
    remove_node(head, node);
    if (where != NULL)
        add_node_after(node, where);
    else {
        node->next = *head;
        (*head)->prev = node;
        *head = node;
    }
}

/*
void printlist(cJSON* p)
{
    sds l = sdsempty();
    for (cJSON* v = p->child; v!= NULL; v = v->next) {
        const cJSON* vp = cJSON_GetArrayItem(v, 0);
        time_t t = str_to_time(cJSON_GetObjectItem(vp, "start")->valuestring);
        l = sdscatprintf(l, " -> [%p] (%p) %s (%ld) ", v->prev, v,
            v->string, (long)t - 1571720000);
    }
    fprintf(stderr, "LIST: %s\n", l);
    sdsfree(l);
}
*/
#define CALL_START(vv) (cJSON_GetObjectItem(cJSON_GetArrayItem((vv), 0), "start")->valuestring)
#define CALL_START_TIME(vv) (str_to_time(CALL_START(vv)))
#define CALL_END(vv) (cJSON_GetObjectItem(cJSON_GetArrayItem((vv), 0), "end")->valuestring)
#define CALL_END_TIME(vv) (str_to_time(CALL_END(vv)))

#define QS_RUN "text"
int main(int argc, char* argv[])
{

    sds run_id = NULL;
    
    if (argc > 1) 
        run_id = sdsnew(argv[1]);
    else {
        const char* qs = getenv("QUERY_STRING");
        int count = 0;
        sds* params = sdssplitlen(qs, strlen(qs), "&", 1, &count);
        if (params == NULL)
            return -1;
        for (int i=0; i<count; ++i) {
            char* p = strchr(params[i], '=');
            *p = '\0';
            if (strcmp(params[i], QS_RUN) != 0)
                continue;
            run_id = sdsnew(p+1);
            break;
        }
        sdsfreesplitres(params, count);
    }
    if (run_id == NULL || sdslen(run_id) == 0)
        return -1;
    /* fprintf(stderr, "RUN_ID='%s'\n", run_id); */

    cJSON* json = getProgress(run_id);

    cJSON* calls = cJSON_GetObjectItemCaseSensitive(json, "calls");
    cJSON* call;

    /* Sort "calls" by "start" datetime */
    for (call = calls->child; call!= NULL; call = call->next) {
        time_t t = CALL_START_TIME(call);
        cJSON* p;
        for (p = call->prev; p!= NULL && CALL_START_TIME(p) > t; p = p->prev);
        if (p == call->prev)
            continue;

        cJSON* next_iter = call->prev;
        move_node_after(&calls->child, call, p);
        call = next_iter;
    }
 
    printf("content-type: application/json\n\n");

    sds progress = sdsempty();
    cJSON_ArrayForEach(call, calls) {
        const cJSON* v = cJSON_GetArrayItem(call, 0);
        double delta = time_delta(CALL_START(call), CALL_END(call));

        sds seconds = sdscatprintf(sdsempty(), "%.f", delta);
        progress = sdscatprintf(progress, "%30s\t%8s\t%s\t%s\t%s secs\n", call->string, 
            cJSON_GetObjectItem(v, "executionStatus")->valuestring,
            CALL_START(call), CALL_END(call), seconds);
        sdsfree(seconds);
    }

    const char* status = cJSON_GetObjectItem(json, "status")->valuestring;
    progress = sdscatprintf(sdsempty(), 
            "The status of run `%s` is *%s*\n"
            "Progress:\n"
            "```\n"
            "%s"
            "\n```", 
            run_id, status, progress);

    cJSON* respjs = cJSON_CreateObject();
    cJSON_AddItemToObject(respjs, "response_type",  cJSON_CreateString("in_channel"));
    cJSON_AddItemToObject(respjs, "text",  cJSON_CreateString(progress));
    
    char* pprint = cJSON_Print(respjs);
    printf("%s", pprint);
    cJSON_free(pprint);

    cJSON_Delete(json);
    cJSON_Delete(respjs);
    sdsfree(progress);
    sdsfree(run_id);
    return 0;
}
