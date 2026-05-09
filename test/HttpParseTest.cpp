#include <iostream>
#include <string>
#include <cassert>
#include "HttpRequest.h"
#include "MultipartParser.h"

int main() {
    const std::string boundary = "----WebKitFormBoundaryTest";
    const std::string multipartBody = "--" + boundary + "\r\n"
                                      "Content-Disposition: form-data; name=\"username\"\r\n"
                                      "\r\n"
                                      "moran\r\n"
                                      "--" + boundary + "\r\n"
                                      "Content-Disposition: form-data; name=\"directory\"\r\n"
                                      "\r\n"
                                      "docs\r\n"
                                      "--" + boundary + "\r\n"
                                      "Content-Disposition: form-data; name=\"file\"; filename=\"hello.txt\"\r\n"
                                      "Content-Type: text/plain\r\n"
                                      "\r\n"
                                      "hello file\r\n"
                                      "--" + boundary + "--\r\n";
    const std::string multipartHeader = "POST /upload HTTP/1.1\r\n"
                                        "Host: 127.0.0.1:8888\r\n"
                                        "Content-Type: multipart/form-data; boundary=" +
                                        boundary + "\r\n"
                                                   "Content-Length: " +
                                        std::to_string(multipartBody.size()) +
                                        "\r\n"
                                        "Connection: close\r\n"
                                        "\r\n";
    const std::string chunk1 = multipartHeader + multipartBody.substr(0, 91);
    const std::string chunk2 = multipartBody.substr(91);

    HttpRequest multipartRequest;
    int consumed = multipartRequest.Parse(chunk1);
    assert(consumed > 0);
    assert(!multipartRequest.IsFinish());

    std::string unread = chunk1.substr(static_cast<size_t>(consumed)) + chunk2;
    consumed = multipartRequest.Parse(unread);
    assert(consumed == static_cast<int>(unread.size()));
    assert(multipartRequest.IsFinish());
    assert(multipartRequest.GetBody() == multipartBody);

    std::vector<MultipartPart> parts;
    std::string errorMsg;
    assert(MultipartParser::Parse(multipartRequest.GetHeader("Content-Type"), multipartRequest.GetBody(), parts, errorMsg));
    assert(parts.size() == 3);
    assert(parts[0].name == "username" && parts[0].content == "moran");
    assert(parts[1].name == "directory" && parts[1].content == "docs");
    assert(parts[2].name == "file" && parts[2].filename == "hello.txt" && parts[2].content == "hello file");

    std::string req_part1 = "GET /hello?a=2 HTTP/1.1\r\n"
                            "Host: 127.0.0.1:1234\r\n"
                            "Connection: keep-alive\r\n"
                            "Cache-Control: max-age=0\r\n"
                            "sec-ch-ua: \"Google Chrome\";v=\"113\", \"Chromium\";v=\"113\", \"Not-A.Brand\";v=\"24\"\r\n"
                            "sec-ch-ua-mobile: ?0\r\n"
                            "sec-ch-ua-platform: \"Linux\"\r\n"
                            "Upgrade-Insecure-Requests: 1\r\n";

    std::string req_part2 =
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/113.0.0.0 Safari/537.36\r\n"
        "Accept: "
        "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/"
        "signed-exchange;v=b3;q=0.7\r\n"
        "Sec-Fetch-Site: none\r\n"
        "Sec-Fetch-Mode: navigate\r\n"
        "Sec-Fetch-User: ?1\r\n"
        "Sec-Fetch-Dest: document\r\n"
        "Accept-Encoding: gzip, deflate, br\r\n"
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,zh-TW;q=0.7\r\n"
        "Cookie: "
        "username-127-0-0-1-8888=\"2|1:0|10:1681994652|23:username-127-0-0-1-8888|44:Yzg5ZjA1OGU0MWQ1NGNlMWI2MGQwYTFhMDAxYzY3YzU=|"
        "6d0b051e144fa862c61464acf2d14418d9ba26107549656a86d92e079ff033ea\"; _xsrf=2|dd035ca7|e419a1d40c38998f604fb6748dc79a10|168199465\r\n"
        "\r\n";
    HttpRequest request;
    auto parseResult = request.Parse(req_part1);
    auto parseResult2 = request.Parse(req_part2);

    std::cout << "parseResult: " << parseResult << std::endl;
    std::cout << "parseResult2: " << parseResult2 << std::endl;

    std::cout << "method: " << request.GetMethodString() << std::endl << std::endl;
    std::cout << "url: " << request.GetUrl() << std::endl << std::endl;
    std::cout << "request_params: " << std::endl;
    for (auto it : request.GetRequestParameters()) {
        std::cout << "key:   " << it.first << std::endl << "value: " << it.second << std::endl << std::endl;
    }
    std::cout << "protocol: " << request.GetProtocol() << std::endl << std::endl;
    std::cout << "version: " << request.GetVersionString() << std::endl << std::endl;

    std::cout << "headers: " << std::endl;
    for (auto it : request.GetHeaders()) {
        std::cout << "key:   " << it.first << std::endl << "value: " << it.second << std::endl;
    }
}
