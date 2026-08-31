#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "files.h"
#include "harness.h"

static void given_tree(char *root_out, size_t cap) {
    char base[] = "/tmp/httpc-test-XXXXXX";
    char *dir = mkdtemp(base);

    char path[512];
    snprintf(path, sizeof(path), "%s/www", dir);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), "%s/www/index.html", dir);
    FILE *f = fopen(path, "w");
    fputs("<h1>ok</h1>", f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/www/style.css", dir);
    f = fopen(path, "w");
    fputs("body{}", f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/segredo.txt", dir);
    f = fopen(path, "w");
    fputs("SEGREDO", f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/www/escape", dir);
    symlink("../segredo.txt", path);

    snprintf(root_out, cap, "%s/www", dir);
}

void test_files(void) {
    char root[512];
    given_tree(root, sizeof(root));

    file_content content;

    TEST("loads an existing file");
    {
        CHECK_INT(file_load(root, "/index.html", &content), FILE_OK);
        CHECK_STR(content.data, "<h1>ok</h1>");
        CHECK_INT(content.len, 11);
        CHECK_STR(content.media_type, "text/html; charset=utf-8");
        file_content_free(&content);
    }

    TEST("root serves index.html");
    {
        CHECK_INT(file_load(root, "/", &content), FILE_OK);
        CHECK_STR(content.data, "<h1>ok</h1>");
        file_content_free(&content);
    }

    TEST("query string is not part of the path");
    {
        CHECK_INT(file_load(root, "/index.html?v=2", &content), FILE_OK);
        file_content_free(&content);
    }

    TEST("missing file is 404, not 403");
    {
        CHECK_INT(file_load(root, "/nao-existe.html", &content), FILE_NOT_FOUND);
    }

    TEST("traversal via ../ is blocked");
    {
        CHECK_INT(file_load(root, "/../segredo.txt", &content), FILE_FORBIDDEN);
        CHECK_INT(file_load(root, "/a/b/../../../segredo.txt", &content), FILE_FORBIDDEN);
        CHECK_INT(file_load(root, "/../../../../etc/passwd", &content), FILE_FORBIDDEN);
    }

    TEST("percent-encoded traversal is blocked");
    {

        CHECK_INT(file_load(root, "/%2e%2e/segredo.txt", &content), FILE_FORBIDDEN);
        CHECK_INT(file_load(root, "/%2E%2E/segredo.txt", &content), FILE_FORBIDDEN);
        CHECK_INT(file_load(root, "/..%2fsegredo.txt", &content), FILE_FORBIDDEN);
    }

    TEST("symlink pointing outside is blocked");
    {

        CHECK_INT(file_load(root, "/escape", &content), FILE_FORBIDDEN);
    }

    TEST("invalid percent-encoding is rejected");
    {
        CHECK_INT(file_load(root, "/%zz", &content), FILE_BAD_TARGET);
        CHECK_INT(file_load(root, "/%2", &content), FILE_BAD_TARGET);
        CHECK_INT(file_load(root, "/a%00b", &content), FILE_BAD_TARGET);
    }

    TEST("media type from extension");
    {
        CHECK_STR(file_media_type("/a/b.html"), "text/html; charset=utf-8");
        CHECK_STR(file_media_type("/a/b.css"),  "text/css; charset=utf-8");
        CHECK_STR(file_media_type("/a/b.json"), "application/json");
        CHECK_STR(file_media_type("/a/b.png"),  "image/png");
        CHECK_STR(file_media_type("/sem-ponto"), "application/octet-stream");
    }

    TEST("free is safe on empty content");
    {
        file_content vazio = {NULL, 0, NULL};
        file_content_free(&vazio);
        file_content_free(&vazio);
        CHECK(vazio.data == NULL);
    }
}
