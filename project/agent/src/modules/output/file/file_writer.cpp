#include "output_file/file_writer.hpp"

namespace piguard::output {

FileWriter::FileWriter(foundation::ThreadSafeQueue<foundation::Event>& event_queue) : event_queue_(event_queue) {}

std::string FileWriter::name() const { return "FileWriter"; }

bool FileWriter::start() {
    running_.store(true);
    return true;
}

void FileWriter::stop() { running_.store(false); }

void FileWriter::flush_once() {
    if (!running_.load()) {
        return;
    }
    (void)event_queue_.pop();
}

}  // namespace piguard::output
