#include "config.hpp"

config config::load(memory_allocator a, memory_block content)
{
    config result;
    acf cfg = acf::parse(a, content);
    result.wait_timeout = cfg.get_value("wait_timeout").to_integer();
    result.backlog_size = cfg.get_value("backlog_size").to_integer();
    result.prune_timeout = cfg.get_value("prune_timeout").to_integer();
    result.unix_domain_socket = cfg.get_value("unix_domain_socket").to_string();
    result.logger.stream = cfg.get_value("logger").get_value("stream").to_bool();
    result.logger.file = cfg.get_value("logger").get_value("file").to_bool();
    result.logger.filename = cfg.get_value("logger").get_value("filename").to_string();
    result.logger.max_size = cfg.get_value("logger").get_value("max_size").to_integer();
    result.logger.something_else.one_ = cfg.get_value("logger").get_value("something_else").get_value("one_").to_integer();
    result.logger.something_else.two_ = cfg.get_value("logger").get_value("something_else").get_value("two_").to_integer();
    result.logger.something_else.three = cfg.get_value("logger").get_value("something_else").get_value("three").to_integer();
    return result;
}
