// Read lines from standard input, interpreting each as a file path.
//
// If the path does not exist, print an error.
//
// If the path exists and is a regular file, print the SHA256 digest of the
// file's contents.  Produce a single tracing span indicating the calculation.
//
// If the path exists and is a directory, calculate the SHA256 digest of the
// directory from the names and digests of its children, combined in some
// canonical format.  Produce a trace whose structure reflects the directory
// structure.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <datadog/opentracing.h>
#include <datadog/tags.h>
#include <filesystem.h> // `namespace fs` is `std::filesystem` or a backport
#include <iostream>
#include <picosha2.h> // `picosha2::hash256` functions
#include <string>
#include <vector>

using Digest = std::array<char, picosha2::k_digest_size>;

// Return the specified `digest` formatted as a lower case hexadecimal string.
std::string hex(const Digest &digest) {
  std::string result;
  for (std::size_t i = 0; i < digest.size(); ++i) {
    char buf[2 + 1];
    std::snprintf(buf, sizeof buf, "%02x",
                  static_cast<unsigned char>(digest[i]));
    result.append(buf, 2);
  }
  return result;
}

// Store into the specified `digest` the SHA256 digest of the contents of the
// specified `file`.  Return zero on success, or a nonzero value if an error
// occurs.
int sha256(Digest &digest, const fs::path &file) {
  std::ifstream in(file);
  if (!in) {
    return 1;
  }
  picosha2::hash256(in, digest.begin(), digest.end());
  return 0;
}

// Return the SHA256 digest of a directory having the specified `children`.
// This function will sort  `children` in place.
Digest sha256(std::vector<std::pair<fs::path, Digest>> &children) {
  std::sort(children.begin(), children.end());

  std::vector<char> descriptor;
  for (const auto &record : children) {
    const std::string path = record.first.filename().u8string();
    const Digest &hash = record.second;
    descriptor.insert(descriptor.end(), path.begin(), path.end());
    descriptor.insert(descriptor.end(), hash.begin(), hash.end());
  }

  Digest digest;
  picosha2::hash256(descriptor, digest);
  return digest;
}

// Store into the specified `digest` the SHA256 digest of the contents of the
// file/directory at the specified `path`, using the tracing context of the
// specified `tracer` and `context`.  Return zero on success.  Return a
// nonzero value if `path` is neither a directory nor a normal file, or if an
// error occurs.
int sha256_traced(Digest &digest, const fs::path &path, ot::Tracer &tracer,
                  const ot::SpanContext &context) try {
  if (fs::is_directory(path)) {
    // Directory: Calculate hash of children, and then combine them.
    const auto span =
        tracer.StartSpan("sha256.directory", {ot::ChildOf(&context)});
    span->SetTag("path", path.u8string());
    span->SetTag("file_name", path.u8string());
    span->SetTag("directory_name", path.u8string());

    std::vector<std::pair<fs::path, Digest>> children;
    const auto options = fs::directory_options::skip_permission_denied;
    for (const auto &entry : fs::directory_iterator(path, options)) {
      if (!fs::is_symlink(path)) {
        Digest hash;
        if (sha256_traced(hash, entry, tracer, span->context()) == 0) {
          children.emplace_back(entry, hash);
        }
      }
    }
    span->SetTag("number_of_children_included", children.size());
    digest = sha256(children);
    span->SetTag("sha256_hex", hex(digest));
    return 0;
  } else if (fs::is_regular_file(path)) {
    // Regular file: Calculate hash of file contents.
    const auto span = tracer.StartSpan("sha256.file", {ot::ChildOf(&context)});
    span->SetTag("path", path.u8string());
    span->SetTag("file_name", path.u8string());
    span->SetTag("file_size_bytes", fs::file_size(path));
    const int rc = sha256(digest, path);
    if (rc) {
      span->SetTag("error", "Unable to calculate sha256 hash.");
    } else {
      span->SetTag("sha256_hex", hex(digest));
    }
    return rc;
  } else {
    // Other kind of file (neither directory nor regular file): Ignore.
    return 1;
  }
} catch (const fs::filesystem_error &) {
  return 1;
} catch (const std::ios_base::failure &) {
  return 1;
}

int main() {
  const auto tracer = datadog::opentracing::makeTracer(
      datadog::opentracing::TracerOptions{"dd-agent", 8126, "example"});

  const std::string prompt = "enter a file or directory (ctrl+d to quit): ";
  std::string input_path;
  while (std::cout << prompt << std::flush,
         std::getline(std::cin, input_path)) {
    const fs::path path(input_path);

    // Create a root span for the current request.
    const auto root = tracer->StartSpan("sha256.request");
    root->SetTag(datadog::tags::environment, "production");
    root->SetTag("path", path.u8string());

    if (!fs::exists(path)) {
      std::cerr << "The file " << path << " does not exist.\n";
      root->SetTag("error", "The file does not exist.");
      continue;
    }

    Digest digest;
    if (sha256_traced(digest, path, *tracer, root->context())) {
      std::cerr << "Unable to calculate the sha256 hash of " << path << ".\n";
      root->SetTag("error", "Unable to calculate sha256 hash.");
    } else {
      const std::string hex_digest = hex(digest);
      root->SetTag("sha256_hex", hex_digest);
      std::cout << "sha256(" << path << "): " << hex_digest << std::endl;
    }
  }

  std::cout << "\n";
  tracer->Close();
}
