#include "util/enums.hh"

namespace amadeus {

using namespace seastar;

extern const sstring
extension_to_mime_type(sstring extension) {
    if (extension == ".m3u8") {
        return "application/vnd.apple.mpegurl";
    } else if (extension == ".m4s") {
        return "video/iso.segment";
    } else if (extension == ".ts") {
        return "video/mp2t";
    } else if (extension == ".flv") {
        return "flv-application/octet-stream";
    } else if (extension == ".bin") {
        return "flv-application/octet-stream";
    } else if (extension == ".dat") {
        return "flv-application/octet-stream";
    } else if (extension == ".json") {
        return "application/json";
    } else if (extension == ".txt") {
        return "text/plain";
    } else if (extension == ".htm") {
        return "text/html";
    } else if (extension == ".html") {
        return "text/html";
    } else if (extension == ".css") {
        return "text/css";
    } else if (extension == ".js") {
        return "text/javascript";
    } else if (extension == ".gif") {
        return "image/gif";
    } else if (extension == ".jpeg") {
        return "image/jpeg";
    } else if (extension == ".png") {
        return "image/png";
    } else if (extension == ".ico") {
        return "image/x-icon";
    } else if (extension == ".svg") {
        return "image/svg+xml";
    } else if (extension == ".p8") {
        return "application/pkcs8";
    } else if (extension == ".key") {
        return "application/pkcs8";
    } else if (extension == ".p10") {
        return "application/pkcs10";
    } else if (extension == ".csr") {
        return "application/pkcs10";
    } else if (extension == ".cer") {
        return "application/pkix-cert";
    } else if (extension == ".crl") {
        return "application/pkix-crl";
    } else if (extension == ".p7c") {
        return "application/pkcs7-mime";
    } else if (extension == ".p8") {
        return "application/pkcs8";
    } else if (extension == ".crt") {
        return "application/x-x509-ca-cert";
    } else if (extension == ".der") {
        return "application/x-x509-ca-cert";
    } else if (extension == ".crl") {
        return "application/application/x-pkcs7-crl";
    } else if (extension == ".pem") {
        return "application/x-pem-file";
    } else if (extension == ".p12") {
        return "application/x-pkcs12";
    } else if (extension == ".pfx") {
        return "application/x-pkcs12";
    } else if (extension == ".p7b") {
        return "application/x-pkcs7-certificates";
    } else if (extension == ".spc") {
        return "application/x-pkcs7-certificates";
    } else if (extension == ".p7r") {
        return "application/x-pkcs7-certreqresp";
    } else if (extension == ".aac") {
        return "audio/aac";
    } else if (extension == ".abw") {
        return "application/x-abiword";
    } else if (extension == ".arc") {
        return "application/x-freearc";
    } else if (extension == ".avif") {
        return "image/avif";
    } else if (extension == ".avi") {
        return "video/x-msvideo";
    } else if (extension == ".azw") {
        return "application/vnd.amazon.ebook";
    } else if (extension == ".bin") {
        return "application/octet-stream";
    } else if (extension == ".bmp") {
        return "image/bmp";
    } else if (extension == ".bz") {
        return "application/x-bzip";
    } else if (extension == ".bz2") {
        return "application/x-bzip2";
    } else if (extension == ".cda") {
        return "application/x-cdf";
    } else if (extension == ".csh") {
        return "application/x-csh";
    } else if (extension == ".css") {
        return "text/css";
    } else if (extension == ".csv") {
        return "text/csv";
    } else if (extension == ".doc") {
        return "application/msword";
    } else if (extension == ".docx") {
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    } else if (extension == ".eot") {
        return "application/vnd.ms-fontobject";
    } else if (extension == ".epub") {
        return "application/epub+zip";
    } else if (extension == ".gz") {
        return "application/gzip";
    } else if (extension == ".gif") {
        return "image/gif";
    } else if (extension == ".htm") {
        return "text/html";
    } else if (extension == ".ico") {
        return "image/vnd.microsoft.icon";
    } else if (extension == ".ics") {
        return "text/calendar";
    } else if (extension == ".jar") {
        return "application/java-archive";
    } else if (extension == ".jpeg") {
        return "image/jpeg";
    } else if (extension == ".js") {
        return "text/javascript";
    } else if (extension == ".json") {
        return "application/json";
    } else if (extension == ".jsonld") {
        return "application/ld+json";
    } else if (extension == ".mid") {
        return "audio/midi";
    } else if (extension == ".mjs") {
        return "text/javascript";
    } else if (extension == ".mp3") {
        return "audio/mpeg";
    } else if (extension == ".mp4") {
        return "video/mp4";
    } else if (extension == ".mpeg") {
        return "video/mpeg";
    } else if (extension == ".mpkg") {
        return "application/vnd.apple.installer+xml";
    } else if (extension == ".odp") {
        return "application/vnd.oasis.opendocument.presentation";
    } else if (extension == ".ods") {
        return "application/vnd.oasis.opendocument.spreadsheet";
    } else if (extension == ".odt") {
        return "application/vnd.oasis.opendocument.text";
    } else if (extension == ".oga") {
        return "audio/ogg";
    } else if (extension == ".ogv") {
        return "video/ogg";
    } else if (extension == ".ogx") {
        return "application/ogg";
    } else if (extension == ".opus") {
        return "audio/opus";
    } else if (extension == ".otf") {
        return "font/otf";
    } else if (extension == ".png") {
        return "image/png";
    } else if (extension == ".pdf") {
        return "application/pdf";
    } else if (extension == ".php") {
        return "application/x-httpd-php";
    } else if (extension == ".ppt") {
        return "application/vnd.ms-powerpoint";
    } else if (extension == ".pptx") {
        return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    } else if (extension == ".rar") {
        return "application/vnd.rar";
    } else if (extension == ".rtf") {
        return "application/rtf";
    } else if (extension == ".sh") {
        return "application/x-sh";
    } else if (extension == ".svg") {
        return "image/svg+xml";
    } else if (extension == ".tar") {
        return "application/x-tar";
    } else if (extension == ".tif") {
        return "image/tiff";
    } else if (extension == ".ts") {
        return "video/mp2t";
    } else if (extension == ".ttf") {
        return "font/ttf";
    } else if (extension == ".txt") {
        return "text/plain";
    } else if (extension == ".vsd") {
        return "application/vnd.visio";
    } else if (extension == ".wav") {
        return "audio/wav";
    } else if (extension == ".weba") {
        return "audio/webm";
    } else if (extension == ".webm") {
        return "video/webm";
    } else if (extension == ".webp") {
        return "image/webp";
    } else if (extension == ".woff") {
        return "font/woff";
    } else if (extension == ".woff2") {
        return "font/woff2";
    } else if (extension == ".xhtml") {
        return "application/xhtml+xml";
    } else if (extension == ".xls") {
        return "application/vnd.ms-excel";
    } else if (extension == ".xlsx") {
        return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    } else if (extension == ".xml") {
        return "application/xml";
    } else if (extension == ".xul") {
        return "application/vnd.mozilla.xul+xml";
    } else if (extension == ".zip") {
        return "application/zip";
    } else if (extension == ".3gp") {
        return "video/3gpp";
    } else if (extension == ".3g2") {
        return "video/3gpp2";
    } else if (extension == ".7z") {
        return "application/x-7z-compressed";
    } else if (extension == ".yaml") {
        return "text/vnd.yaml";
    } else {
        return "";
    }
}

std::ostream &
operator<<(std::ostream &os, const type_t v) {
    return os << type_to_string(v);
}

std::ostream &
operator<<(std::ostream &os, const format_t v) {
    return os << format_to_string(v);
}

std::ostream &
operator<<(std::ostream &os, const protocol_t v) {
    return os << protocol_to_string(v);
}

std::ostream &
operator<<(std::ostream &os, const ownership_t v) {
    return os << ownership_to_string(v);
}

std::ostream &
operator<<(std::ostream &os, const media_type_t v) {
    return os << media_type_to_short_string(v);
}

} // namespace amadeus
