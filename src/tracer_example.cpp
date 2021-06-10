// Read lines from standard input.  Interpret each line as the path to a
// directory.
//
// For each such directory, calculate a checksum for it by recursively visiting
// its descendants and summing all of the bytes of regular files.
//
// Also, for each such directory, produce a trace whose structure reflects that
// of the directory tree.

#include <cstdint>
#include <datadog/opentracing.h>
#include <datadog/tags.h>
#include <filesystem.h> // `namespace fs` is `std::filesystem` or a backport
#include <iostream>
#include <string>

// Return the sum of all of the bytes in the specified regular `file`, modulo
// pow(2, 64).  Return zero if an error occurs.
std::uint64_t checksum64_file(const fs::path &file) {
  std::uint64_t sum = 0;
  std::ifstream in(file);
  char ch;
  while (in.get(ch)) {
    sum += static_cast<std::uint64_t>(ch);
  }

  return sum;
}

// Return the checksum64 of the specified `path`, using the tracing context of
// the specified `tracer` and `context`.  Return zero if `path` is neither a
// directory nor a normal file, or if an error occurs.
std::uint64_t checksum64_traced(const fs::path &path, ot::Tracer &tracer,
                                const ot::SpanContext &context) try {
  if (fs::is_directory(path)) {
    const auto span =
        tracer.StartSpan("checksum64.directory", {ot::ChildOf(&context)});
    span->SetTag("file_name", path.string());
    span->SetTag("directory_name", path.string());
    std::uint64_t number_of_children_included = 0;
    std::uint64_t total = 0;
    const auto options = fs::directory_options::skip_permission_denied;
    for (const auto &entry : fs::directory_iterator(path, options)) {
      if (!fs::is_symlink(path)) {
        ++number_of_children_included;
        total += checksum64_traced(entry, tracer, span->context());
      }
    }
    span->SetTag("checksum64", total);
    span->SetTag("number_of_children_included", number_of_children_included);
    return total;
  } else if (fs::is_regular_file(path)) {
    const auto span =
        tracer.StartSpan("checksum64.file", {ot::ChildOf(&context)});
    span->SetTag("file_name", path.string());
    span->SetTag("file_size_bytes", fs::file_size(path));
    const std::uint64_t total = checksum64_file(path);
    span->SetTag("checksum64", total);
    return total;
  } else {
    return 0;
  }
} catch (const fs::filesystem_error &) {
  return 0;
}

int main() {
  const auto tracer = datadog::opentracing::makeTracer(
      datadog::opentracing::TracerOptions{"dd-agent", 8126, "example"});

  const std::string prompt = "enter a directory (ctrl+d to quit): ";
  std::string directory_path;
  while (std::cout << prompt << std::flush,
         std::getline(std::cin, directory_path)) {
    const fs::path path(directory_path);

    if (!fs::exists(path)) {
      std::cerr << "The file " << path << " does not exist.\n";
      continue;
    }
    if (!fs::is_directory(path)) {
      std::cerr << path << " is not a directory.\n";
      continue;
    }

    // Create a root span for the current request.
    const auto root = tracer->StartSpan("checksum64.request");
    root->SetTag(datadog::tags::environment, "production");
    root->SetTag("directory_name", path.string());
    const std::uint64_t checksum =
        checksum64_traced(path, *tracer, root->context());
    root->SetTag("checksum64", checksum);
    std::cout << "checksum64(" << path << "): " << checksum << std::endl;
  }

  std::cout << "\n";
  tracer->Close();
}
