The code in question in the callback http session:
```c++
void callback_http_session::on_write(std::size_t sequence, beast::error_code ec, std::size_t _) {
    ...
    auto it = pending_responses_.find(sequence);
    bool close_after_write {true};
    if (it != pending_responses_.end()) {                  // here it->second.close_after_write = true           
        pending_responses_.erase(sequence);
        close_after_write = it->second.close_after_write;  // here it->second.close_after_write = false
    } else {
        warp::logging::error("Error in {} during {}: {}", COMPONENT, "on_write{pending_responses.find}",
                             "could not find response in pending response map to erase");
    }
    ...
}
```

when the key that the iterator is pointing to is erased, a different key is there in the iterator's value. This caused the write logic to be incorrect because even though the socket should have been closed, it was not since the value changed in real time for the sequence.

TLDR: undefined behavior, bad container usage. (looks like node reuse by the `std::unordered_map`)
