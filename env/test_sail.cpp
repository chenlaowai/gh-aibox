// Minimal C++ sophon-sail smoke test: load a bmodel through sail::Engine and
// print its graph/input/output metadata.  This proves the C++ SDK built in
// env/setup_sdk.sh is linkable and loadable on the AI box.
#include <iostream>
#include <string>
#include <vector>

#include "sail/engine.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <bmodel>" << std::endl;
        return 2;
    }
    try {
        sail::Engine engine(argv[1], 0, sail::IOMode::SYSIO);
        auto graphs = engine.get_graph_names();
        if (graphs.empty()) {
            std::cerr << "no graph in bmodel" << std::endl;
            return 1;
        }
        const std::string& graph = graphs[0];
        auto inputs = engine.get_input_names(graph);
        auto outputs = engine.get_output_names(graph);
        std::cout << "SAIL_OK graph=" << graph << std::endl;
        for (const auto& name : inputs) {
            auto shape = engine.get_input_shape(graph, name);
            std::cout << "  input " << name << " shape=";
            for (size_t i = 0; i < shape.size(); ++i) std::cout << (i ? "," : "") << shape[i];
            std::cout << std::endl;
        }
        for (const auto& name : outputs) {
            auto shape = engine.get_output_shape(graph, name);
            std::cout << "  output " << name << " shape=";
            for (size_t i = 0; i < shape.size(); ++i) std::cout << (i ? "," : "") << shape[i];
            std::cout << std::endl;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "SAIL_FAIL: " << e.what() << std::endl;
        return 1;
    }
}
