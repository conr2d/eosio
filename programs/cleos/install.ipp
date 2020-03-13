#include <curl/curl.h>

#if defined(__clang__) || (__GNUC__ >= 8)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <fc/filesystem.hpp>
namespace fs = fc;
#endif

size_t curl_writefunc(void *data, size_t size, size_t nmemb, void* userp) {
   ((std::string*)userp)->append((char*)data, size * nmemb);
   return size * nmemb;
}

size_t curl_writefile(void *data, size_t size, size_t nmemb, void* stream) {
   auto ret = fwrite(data, size, nmemb, (FILE*)stream);
   return ret;
}

struct install_subcommand {
   string package_name;
   string package_owner = "EOSIO";
   string version;
   string package_type = "deb";

   install_subcommand(CLI::App* app) {
      auto install = app->add_subcommand("install", localized("Install package"));
      install->add_option("name", package_name, localized("A package name to be installed"))->required();
      install->add_option("--type", package_type, localized("A type of package [deb/rpm], defaults to 'deb'"));

      install->set_callback([this] {
         auto pos = package_name.find('/');
         if (pos != string::npos) {
            package_owner = package_name.substr(0, pos);
            package_name = package_name.substr(pos + 1);
         }
         // hack to manage eoscc
         else if (package_name.find("eoscc") != string::npos) {
            package_owner = "turnpike";
         }
         //

         pos = package_name.find('@');
         if (pos != string::npos) {
            version = package_name.substr(pos + 1);
            package_name = package_name.substr(0, pos);
         }

         std::transform(package_type.begin(), package_type.end(), package_type.begin(), [](unsigned char c) { return std::tolower(c); });

         CURL* curl = curl_easy_init();
         std::string buffer;

         auto url = "https://api.github.com/repos/" + package_owner + "/" + package_name + + "/releases/" + (version.empty() ?  "latest" : "tags/" + version);
         auto user_agent = std::string(cli_client_executable_name) + "/" + eosio::version::version_client().substr(1);

         CURLcode code;
         curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
         curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writefunc);
         curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

         code = curl_easy_perform(curl);

         if (code == CURLE_OK) {
            try {
               auto result = fc::json::json::from_string(buffer).as<fc::variant_object>();
               for (auto asset: result["assets"].get_array()) {
                  url = asset.get_object()["browser_download_url"].as_string();
                  if (url.substr(url.size() - package_type.size()) == package_type) {
                     auto pos = url.rfind("/");
                     auto filename = url.substr(pos + 1);
                     auto tmp = fs::temp_directory_path();
                     fs::create_directories(tmp / "eosio-packages");
                     auto filepath = tmp / "eosio-packages" / filename;
                     auto fout = fopen(filepath.string().c_str(), "wb");

                     std::cout << "Downloading... " << filename << std::endl << std::endl;
                     curl_easy_reset(curl);
                     curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                     curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                     curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
                     curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                     curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
                     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writefile);
                     curl_easy_setopt(curl, CURLOPT_WRITEDATA, fout);

                     code = curl_easy_perform(curl);

                     if (code == CURLE_OK) {
                        std::cout << std::endl << "Installing..." << std::endl;
                        if (package_type == "deb") {
                           auto cmd = "sudo apt install " + filepath.string();
                           system(cmd.c_str());
                        } else if (package_type == "rpm") {
                           auto cmd = "sudo yum install " + filepath.string();
                           system(cmd.c_str());
                        }
                     } else {
                        std::cerr << curl_easy_strerror(code) << std::endl;
                     }
                     break;
                  }
               }
            } FC_CAPTURE_AND_RETHROW()
         } else {
            std::cerr << curl_easy_strerror(code) << std::endl;
         }
         curl_easy_cleanup(curl);
      });
   }
};
