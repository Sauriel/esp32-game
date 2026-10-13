#include "FileServer.h"
#include "Path.h"
#include "FileActions.h"
#include "storage/SdStorage.h"
#include "app/ModeSwitch.h"
#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
namespace FileServer {
namespace {
httpd_handle_t server = nullptr;
constexpr const char* diagnosticsVersion = "file-manager-1";
std::atomic<bool> active{false};
std::atomic<bool> retryAsyncWake{false};
std::atomic<bool> accepting{false};
struct Job { httpd_req_t* request; esp_err_t (*handler)(httpd_req_t*); };
QueueHandle_t jobs = nullptr;
TaskHandle_t worker = nullptr;
std::atomic<uint32_t> completed{0}, total{0};
std::atomic<const char*> message{"Bereit"};
std::atomic<bool> uploading{false};
std::atomic<const char*> uploadPhase{"idle"};
std::atomic<uint32_t> uploadPhaseAt{0}, uploadReceived{0};
uint32_t diagnosticTimeMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void setUploadPhase(const char* phase) {
    uploadPhaseAt.store(diagnosticTimeMs());
    uploadPhase.store(phase);
}
struct UploadLogScope {
    UploadLogScope(size_t length) {
        total.store(length); completed.store(0); uploadReceived.store(0);
        setUploadPhase("validate"); uploading.store(true);
        ESP_LOGI("Upload", "Start: expected=%u bytes", (unsigned)length);
    }
    ~UploadLogScope() { uploading.store(false); }
};
const char* failure = nullptr;
char buffer[4096]; // Ein Server-Task, ein laufender Handler; kein Dateipuffer pro Session.
extern const unsigned char pageStart[] asm("_binary_index_html_start");
extern const unsigned char pageEnd[] asm("_binary_index_html_end");
esp_err_t errorResponse(httpd_req_t* request, const char* code, const char* text) {
    if (!strncmp(request->uri, "/api/action", 11)) {
        ESP_LOGW("FileAction", "HTTP %s: %s", code, text);
    }
    if (request->method == HTTP_POST && !strncmp(request->uri, "/api/file", 9)) {
        ESP_LOGE("Upload", "HTTP %s: %s (phase=%s, received=%u, written=%u)",
            code, text, uploadPhase.load(), (unsigned)uploadReceived.load(), (unsigned)completed.load());
    }
    httpd_resp_set_status(request, code);
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(request, "Connection", "close");
    // Ungelesene Uploaddaten duerfen nicht als naechster Request erscheinen.
    const auto result = httpd_resp_send(request, text, HTTPD_RESP_USE_STRLEN);
    return result == ESP_OK ? ESP_FAIL : result;
}
bool pathFromQuery(httpd_req_t* request, char* path) {
    char query[768], encoded[720];
    const size_t length = httpd_req_get_url_query_len(request);
    if (!length || length >= sizeof(query)) return false;
    return httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "path", encoded, sizeof(encoded)) == ESP_OK &&
        TransferPath::decode(encoded, path, SdStorage::pathCapacity);
}
bool cancelled(int64_t start) {
    return ModeSwitch::shouldAbortTransfer() || esp_timer_get_time() - start > 600000000;
}
// Begrenztes JSON-Escaping, auch fuer nicht vertrauenswuerdige SD-Dateinamen.
bool escape(const char* input, char* output, size_t capacity) {
    size_t n = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(input); *p; ++p) {
        const size_t count = (*p < 32) ? 6 : (*p == '"' || *p == '\\' ? 2 : 1);
        if (n + count >= capacity) return false;
        if (count == 6) { snprintf(output + n, capacity - n, "\\u%04x", *p); n += 6; }
        else { if (count == 2) output[n++] = '\\'; output[n++] = *p; }
    }
    output[n] = 0; return true;
}
esp_err_t index(httpd_req_t* request) {
    if (ModeSwitch::pending()) return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "X-Transfer-Diagnostics", diagnosticsVersion);
    ESP_LOGI("FileServer", "GET /: diagnostics=%s page_bytes=%u", diagnosticsVersion, (unsigned)(pageEnd - pageStart));
    return httpd_resp_send(request, reinterpret_cast<const char*>(pageStart), pageEnd - pageStart);
}
esp_err_t startGame(httpd_req_t* request) {
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    const auto result = httpd_resp_send(request, "Wechsel zum Spiel angefordert", HTTPD_RESP_USE_STRLEN);
    ModeSwitch::request();
    return result;
}
esp_err_t list(httpd_req_t* request) {
    if (ModeSwitch::pending()) return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    if (!SdStorage::ready()) return errorResponse(request, "503 Service Unavailable", "SD nicht verfuegbar");
    char path[SdStorage::pathCapacity];
    if (!pathFromQuery(request, path)) return errorResponse(request, "400 Bad Request", "Ungueltiger Pfad");
    if (!SdStorage::listBegin(path)) return errorResponse(request, "404 Not Found", "Ordner nicht lesbar");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    bool ok = httpd_resp_send_chunk(request, "[", 1) == ESP_OK, first = true;
    char name[SdStorage::nameCapacity], escaped[6 * SdStorage::nameCapacity];
    const int64_t start = esp_timer_get_time();
    int next = 0;
    while (ok && !cancelled(start) && (next = SdStorage::listNext(name, sizeof(name))) > 0) {
        if (!strcmp(name, ".") || !strcmp(name, "..") || TransferPath::temporary(name)) continue;
        char child[SdStorage::pathCapacity];
        const int length = snprintf(child, sizeof(child), "%s%s%s", path, strcmp(path, "/") ? "/" : "", name);
        struct stat info;
        if (length < 0 || length >= int(sizeof(child)) || !SdStorage::statPath(child, info) ||
            !escape(name, escaped, sizeof(escaped))) { ok = false; break; }
        const int size = snprintf(buffer, sizeof(buffer), "%s{\"name\":\"%s\",\"directory\":%s,\"size\":%llu}",
            first ? "" : ",", escaped, S_ISDIR(info.st_mode) ? "true" : "false", (unsigned long long)info.st_size);
        ok = size > 0 && size < int(sizeof(buffer)) && httpd_resp_send_chunk(request, buffer, size) == ESP_OK;
        first = false;
    }
    if (next < 0 || cancelled(start)) ok = false;
    if (!SdStorage::listEnd()) ok = false;
    if (!ok) return ESP_FAIL; // Keine abgeschlossene JSON-Liste bei I/O-Fehler.
    if (httpd_resp_send_chunk(request, "]", 1) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(request, nullptr, 0);
}
esp_err_t download(httpd_req_t* request) {
    if (ModeSwitch::pending()) return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    if (!SdStorage::ready()) return errorResponse(request, "503 Service Unavailable", "SD nicht verfuegbar");
    char path[SdStorage::pathCapacity]; struct stat info;
    if (!pathFromQuery(request, path)) return errorResponse(request, "400 Bad Request", "Ungueltiger Pfad");
    if (!SdStorage::statPath(path, info) || !S_ISREG(info.st_mode))
        return errorResponse(request, "404 Not Found", "Datei nicht gefunden");
    const int fd = SdStorage::openRead(path);
    if (fd < 0) return errorResponse(request, "500 Internal Server Error", "Datei nicht lesbar");
    httpd_resp_set_type(request, "application/octet-stream");
    httpd_resp_set_hdr(request, "Content-Disposition", "attachment");
    total.store(info.st_size); completed.store(0); message.store("Download");
    const int64_t start = esp_timer_get_time();
    bool ok = true;
    uint64_t sent = 0;
    while (sent < uint64_t(info.st_size)) {
        if (cancelled(start)) { ok = false; break; }
        const int count = SdStorage::read(fd, buffer, std::min<uint64_t>(sizeof(buffer), info.st_size - sent));
        if (count <= 0 || httpd_resp_send_chunk(request, buffer, count) != ESP_OK) { ok = false; break; }
        sent += count; completed.store(sent);
    }
    if (!SdStorage::close(fd, false)) ok = false;
    message.store(ok ? "Download fertig" : "Download abgebrochen/Lesefehler");
    return ok ? httpd_resp_send_chunk(request, nullptr, 0) : ESP_FAIL;
}
esp_err_t upload(httpd_req_t* request) {
    UploadLogScope diagnostics(request->content_len);
    if (ModeSwitch::pending()) return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    if (!SdStorage::ready()) return errorResponse(request, "503 Service Unavailable", "SD nicht verfuegbar");
    char path[SdStorage::pathCapacity], parent[SdStorage::pathCapacity], temp[SdStorage::pathCapacity];
    if (!pathFromQuery(request, path) || !strcmp(path, "/"))
        return errorResponse(request, "400 Bad Request", "Ungueltiger Dateipfad");
    ESP_LOGI("Upload", "Target: %s", path);
    struct stat info;
    setUploadPhase("stat-target");
    if (SdStorage::statPath(path, info)) return errorResponse(request, "409 Conflict", "Datei existiert bereits");
    if (errno != ENOENT) {
        ESP_LOGE("Upload", "Target stat failed: errno=%d", errno);
        return errorResponse(request, "500 Internal Server Error", "Ziel kann nicht geprueft werden");
    }
    strcpy(parent, path); char* slash = strrchr(parent, '/');
    if (slash == parent) parent[1] = 0; else *slash = 0;
    setUploadPhase("stat-parent");
    if (!SdStorage::statPath(parent, info) || !S_ISDIR(info.st_mode))
        return errorResponse(request, "404 Not Found", "Zielordner fehlt");
    // Reservierter Name im selben Ordner. O_EXCL bewahrt auch Stromausfallreste.
    int fd = -1;
    setUploadPhase("create-temp");
    for (unsigned i = 0; i < 8 && fd < 0; ++i) {
        const int length = snprintf(temp, sizeof(temp), "%s%s.transfer-%08x.part", parent,
            strcmp(parent, "/") ? "/" : "", (unsigned)(esp_timer_get_time() + i));
        if (length < 0 || length >= int(sizeof(temp)))
            return errorResponse(request, "400 Bad Request", "Pfad zu lang fuer temporaere Datei");
        fd = SdStorage::create(temp);
        const int createError = errno;
        if (fd < 0) ESP_LOGW("Upload", "Temp create failed: attempt=%u errno=%d", i + 1, createError);
        if (fd < 0 && createError != EEXIST) break;
    }
    if (fd < 0) return errorResponse(request, "507 Insufficient Storage", "Temporaere Datei nicht anlegbar");
    ESP_LOGI("Upload", "Temp opened: %s fd=%d; streaming buffer=%u bytes", temp, fd, (unsigned)sizeof(buffer));
    total.store(request->content_len); completed.store(0); message.store("Upload");
    const int64_t start = esp_timer_get_time();
    size_t received = 0;
    bool ok = true;
    while (received < request->content_len) {
        if (cancelled(start)) {
            ESP_LOGW("Upload", "Cancelled: reason=%s written=%u/%u",
                ModeSwitch::shouldAbortTransfer() ? "mode-switch" : "total-timeout",
                (unsigned)received, (unsigned)request->content_len);
            ok = false; break;
        }
        setUploadPhase("http-recv");
        const int count = httpd_req_recv(request, buffer, std::min<size_t>(sizeof(buffer), request->content_len - received));
        if (count <= 0) {
            ESP_LOGE("Upload", "Receive failed: result=%d errno=%d reason=%s received=%u/%u", count, errno,
                count == HTTPD_SOCK_ERR_TIMEOUT ? "socket-timeout" : (count == 0 ? "peer-closed" : "socket-error"),
                (unsigned)uploadReceived.load(), (unsigned)request->content_len);
            ok = false; break;
        }
        uploadReceived.fetch_add(count);
        if (received == 0) ESP_LOGI("Upload", "First receive: %d bytes; starting SD write", count);
        setUploadPhase("sd-write");
        const int written = SdStorage::write(fd, buffer, count);
        if (written != count) {
            if (written > 0) completed.fetch_add(written);
            ESP_LOGE("Upload", "Write failed: expected=%d actual=%d errno=%d offset=%u", count, written, errno, (unsigned)received);
            ok = false; break;
        }
        if (received == 0) ESP_LOGI("Upload", "First SD write complete: %d bytes", written);
        received += count; completed.store(received);
    }
    setUploadPhase("sync-close");
    ESP_LOGI("Upload", "Sync/close begin: written=%u/%u", (unsigned)received, (unsigned)request->content_len);
    const bool closed = SdStorage::close(fd, true);
    ESP_LOGI("Upload", "Sync/close complete: success=%d", closed);
    if (!closed) ok = false;
    if (ok && received == request->content_len && !cancelled(start)) {
        setUploadPhase("rename");
        ESP_LOGI("Upload", "Rename begin: %s -> %s", temp, path);
        ok = SdStorage::rename(temp, path);
        if (!ok) ESP_LOGE("Upload", "Rename failed: errno=%d", errno);
    }
    else ok = false;
    if (!ok) {
        setUploadPhase("cleanup");
        ESP_LOGW("Upload", "Cleanup begin: %s", temp);
        if (!SdStorage::remove(temp)) ESP_LOGE("FileServer", "Temporaere Uploaddatei konnte nicht entfernt werden");
        else ESP_LOGI("Upload", "Temp removed");
        message.store("Upload abgebrochen/Schreibfehler");
        return errorResponse(request, "500 Internal Server Error", "Upload fehlgeschlagen; Ziel nicht veroeffentlicht");
    }
    message.store("Upload fertig");
    ESP_LOGI("Upload", "Saved: %s bytes=%u elapsed_ms=%u", path, (unsigned)received,
        (unsigned)((esp_timer_get_time() - start) / 1000));
    setUploadPhase("http-response");
    httpd_resp_set_status(request, "201 Created");
    const auto response = httpd_resp_send(request, "Datei gespeichert", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("Upload", "Response complete: %s", esp_err_to_name(response));
    return response;
}
esp_err_t fileAction(httpd_req_t* request) {
    if (ModeSwitch::pending()) return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    if (!SdStorage::ready()) return errorResponse(request, "503 Service Unavailable", "SD nicht verfuegbar");
    total.store(0); completed.store(0); message.store("Dateiaktion");
    // Beide URL-kodierten Pfade passen in einen begrenzten Formular-Body;
    // die URI bleibt kurz. Derselbe einzelne Worker besitzt den Puffer.
    if (!request->content_len || request->content_len >= 1536)
        return errorResponse(request, "400 Bad Request", "Formular fehlt oder ist zu lang");
    size_t received = 0;
    const int64_t start = esp_timer_get_time();
    while (received < request->content_len) {
        if (cancelled(start)) return errorResponse(request, "503 Service Unavailable", "Dateiaktion abgebrochen");
        const int count = httpd_req_recv(request, buffer + received, request->content_len - received);
        if (count <= 0) {
            ESP_LOGW("FileAction", "Form receive failed: result=%d errno=%d", count, errno);
            return errorResponse(request, "400 Bad Request", "Formular unvollstaendig");
        }
        received += count;
    }
    if (memchr(buffer, 0, received)) return errorResponse(request, "400 Bad Request", "Ungueltiges Formular");
    buffer[received] = 0;
    char action[16], encoded[720], source[SdStorage::pathCapacity], target[SdStorage::pathCapacity];
    if (httpd_query_key_value(buffer, "action", action, sizeof(action)) != ESP_OK ||
        httpd_query_key_value(buffer, "path", encoded, sizeof(encoded)) != ESP_OK ||
        !TransferPath::decode(encoded, source, sizeof(source)))
        return errorResponse(request, "400 Bad Request", "Ungueltige Aktion oder Quelle");
    FileActions::Action selected;
    if (!strcmp(action, "mkdir")) selected = FileActions::Action::CreateDirectory;
    else if (!strcmp(action, "delete")) selected = FileActions::Action::Delete;
    else if (!strcmp(action, "move")) selected = FileActions::Action::Move;
    else if (!strcmp(action, "rename")) selected = FileActions::Action::Rename;
    else return errorResponse(request, "400 Bad Request", "Unbekannte Dateiaktion");
    const bool relocating = selected == FileActions::Action::Move || selected == FileActions::Action::Rename;
    if (relocating && (httpd_query_key_value(buffer, "target", encoded, sizeof(encoded)) != ESP_OK ||
        !TransferPath::decode(encoded, target, sizeof(target))))
        return errorResponse(request, "400 Bad Request", "Ungueltiger Zielpfad");
    // Nach vollstaendigem Formular, vor der ersten Aenderung erneut pruefen.
    if (ModeSwitch::pending()) return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    const auto result = FileActions::execute(selected, source, relocating ? target : nullptr);
    message.store(result.message);
    if (result.error != FileActions::Error::None) {
        const char* status = result.error == FileActions::Error::Invalid ? "400 Bad Request" :
            (result.error == FileActions::Error::NotFound ? "404 Not Found" :
            (result.error == FileActions::Error::Conflict ? "409 Conflict" : "500 Internal Server Error"));
        return errorResponse(request, status, result.message);
    }
    httpd_resp_set_type(request, "text/plain; charset=utf-8");
    return httpd_resp_send(request, result.message, HTTPD_RESP_USE_STRLEN);
}
void asyncFinishedOnServer(void*) {
    // Erst im HTTP-Task freigeben: folgende Requests sollen nicht noch busy
    // sehen, nachdem ihre Verbindung wieder in select() aufgenommen wurde.
    active.store(false);
    ESP_LOGI("FileServer", "Async completion processed on server; connection ready");
}
void wakeServerAfterAsync() {
    const auto result = httpd_queue_work(server, asyncFinishedOnServer, nullptr);
    if (result != ESP_OK) {
        // Keine neue Operation/Shutdown zulassen, bis die Benachrichtigung
        // gelingt. Arduino-Task kann den Wakeup begrenzt erneut versuchen.
        retryAsyncWake.store(true);
        ESP_LOGE("FileServer", "Async server wakeup failed: %s; retry pending", esp_err_to_name(result));
    }
}
void runWorker(void*) {
    Job job;
    for (;;) {
        if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE) continue;
        ESP_LOGI("FileServer", "Worker begin: operation=%s", job.handler == fileAction ? "action" : (job.handler == upload ? "upload" : (job.handler == list ? "list" : "download")));
        const auto result = job.handler(job.request);
        ESP_LOGI("FileServer", "Worker complete: result=%s", esp_err_to_name(result));
        // Schliesse insbesondere unvollstaendige Requests/Chunk-Antworten.
        // Nicht vor async_complete eine Session-Freigabe auf dem Server
        // einreihen: der kopierte Request besitzt noch einen Sessionzeiger.
        // shutdown beendet I/O, ohne den Descriptor/Sessionplatz freizugeben.
        if (result != ESP_OK) shutdown(httpd_req_to_sockfd(job.request), SHUT_RDWR);
        const auto done = httpd_req_async_handler_complete(job.request);
        if (done != ESP_OK) ESP_LOGE("FileServer", "Async-Abschluss: %s", esp_err_to_name(done));
        // IDF 5.4.2 complete() setzt nur for_async_req=false. Der HTTP-Task
        // kann noch ohne diese Verbindung in select() schlafen. Eine Nachricht
        // ueber seinen Control-Socket weckt ihn und erneuert die Deskriptoren.
        wakeServerAfterAsync();
    }
}
esp_err_t dispatch(httpd_req_t* request) {
    ESP_LOGI("FileServer", "HTTP file request: method=%d content_length=%u socket=%d",
        (int)request->method, (unsigned)request->content_len, httpd_req_to_sockfd(request));
    if (!accepting.load() || ModeSwitch::pending()) return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    if (active.exchange(true)) return errorResponse(request, "503 Service Unavailable", "Dateioperation laeuft bereits");
    // Recheck nach Reservierung verhindert eine Race mit stop().
    if (!accepting.load() || ModeSwitch::pending()) {
        active.store(false);
        return errorResponse(request, "503 Service Unavailable", "Moduswechsel laeuft");
    }
    Job job{};
    job.handler = !strncmp(request->uri, "/api/action", 11) ? fileAction :
        (!strncmp(request->uri, "/api/list", 9) ? list : (request->method == HTTP_POST ? upload : download));
    const auto result = httpd_req_async_handler_begin(request, &job.request);
    ESP_LOGI("FileServer", "Async request begin: result=%s", esp_err_to_name(result));
    if (result != ESP_OK) {
        active.store(false);
        return errorResponse(request, "503 Service Unavailable", "Request-Speicher fehlt");
    }
    if (xQueueSend(jobs, &job, 0) != pdTRUE) {
        errorResponse(job.request, "503 Service Unavailable", "Dateioperation nicht verfuegbar");
        shutdown(httpd_req_to_sockfd(job.request), SHUT_RDWR);
        httpd_req_async_handler_complete(job.request);
        active.store(false);
    } else {
        ESP_LOGI("FileServer", "Request queued for worker");
    }
    return ESP_OK;
}
}
bool begin() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_open_sockets = 2;
    config.max_uri_handlers = 6;
    config.recv_wait_timeout = 2; config.send_wait_timeout = 2;
    config.lru_purge_enable = true;
    jobs = xQueueCreate(1, sizeof(Job));
    if (!jobs || xTaskCreate(runWorker, "TransferFiles", 8192, nullptr, 1, &worker) != pdPASS) {
        if (jobs) vQueueDelete(jobs);
        jobs = nullptr; failure = "HTTP-Worker: zu wenig Speicher"; return false;
    }
    auto result = httpd_start(&server, &config);
    if (result == ESP_OK) {
        const httpd_uri_t routes[] = {
            {"/", HTTP_GET, index, nullptr},
            {"/api/list", HTTP_GET, dispatch, nullptr},
            {"/api/file", HTTP_GET, dispatch, nullptr},
            {"/api/file", HTTP_POST, dispatch, nullptr},
            {"/api/game", HTTP_POST, startGame, nullptr},
            {"/api/action", HTTP_POST, dispatch, nullptr}
        };
        for (const auto& route : routes) {
            result = httpd_register_uri_handler(server, &route);
            if (result != ESP_OK) break;
        }
    }
    if (result != ESP_OK) {
        ESP_LOGE("FileServer", "Serverstart: %s", esp_err_to_name(result));
        failure = "HTTP-Server konnte nicht starten";
        if (server) httpd_stop(server);
        server = nullptr;
        vTaskDelete(worker); worker = nullptr;
        vQueueDelete(jobs); jobs = nullptr;
        return false;
    }
    accepting.store(true);
    ESP_LOGI("FileServer", "HTTP ready: diagnostics=%s compiled=%s %s page_bytes=%u",
        diagnosticsVersion, __DATE__, __TIME__, (unsigned)(pageEnd - pageStart));
    return true;
}
bool stop() {
    accepting.store(false);
    if (active.load()) return false;
    if (failure && server) return false;
    if (server) {
        const auto result = httpd_stop(server);
        if (result != ESP_OK) {
            ESP_LOGE("FileServer", "Shutdown: %s", esp_err_to_name(result));
            failure = "HTTP-Shutdown fehlgeschlagen; kein Neustart";
            return false;
        }
        server = nullptr;
    }
    if (worker) { vTaskDelete(worker); worker = nullptr; }
    if (jobs) { vQueueDelete(jobs); jobs = nullptr; }
    return true;
}
bool busy() { return active.load(); }
Progress progress() { return {active.load(), completed.load(), total.load(), message.load()}; }
const char* error() { return failure; }
void logUploadStatus() {
    // Nur der Arduino-Task liest/schreibt diesen Taktzaehler.
    static uint32_t lastLogAt = 0;
    static uint32_t lastWakeRetryAt = 0;
    const uint32_t now = diagnosticTimeMs();
    if (now - lastWakeRetryAt >= 1000 && retryAsyncWake.exchange(false)) {
        lastWakeRetryAt = now;
        wakeServerAfterAsync();
    }
    if (!uploading.load() || now - lastLogAt < 1000) return;
    lastLogAt = now;
    const uint32_t phaseAt = uploadPhaseAt.load();
    const char* phase = uploadPhase.load();
    ESP_LOGI("Upload", "Progress: received=%u written=%u total=%u phase=%s phase_ms=%u",
        (unsigned)uploadReceived.load(), (unsigned)completed.load(), (unsigned)total.load(),
        phase, (unsigned)(diagnosticTimeMs() - phaseAt));
}
}
