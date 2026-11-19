#include "musdesk/core/CompositionHistory.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    try {
        musdesk::CompositionHistory history;
        musdesk::CompositionSettings original, edited = original;
        history.reset(original);
        require(!history.canUndo() && !history.canRedo(),"New history must be empty");
        for (int i=1; i<=20; ++i) {
            edited.preset.glowWidth = 1+i*0.05f;
            history.observe(edited,true);
        }
        history.observe(edited,false);
        require(history.undo() == original && !history.canUndo(),"A slider gesture must be one undo step");
        require(history.redo() == edited && !history.canRedo(),"Redo must restore the whole edit");
        const auto saved = edited;
        edited.title = "New title";
        edited.labelFraming.zoom = 2;
        edited.preset.spectrumPoints = 64;
        edited.preset.labelsEnabled = false;
        edited.preset.labelsPulse = 0.11f;
        edited.design.scene = musdesk::SceneKind::Cover;
        edited.design.showProgress = false;
        history.observe(edited,false);
        require(history.undo() == saved,"Undo must return to the saved composition exactly");
        edited = saved;
        edited.format = musdesk::CompositionFormat::Square;
        history.observe(edited,false);
        require(!history.canRedo(),"A new edit must discard the old redo branch");
        history.reset(original);
        edited = original;
        edited.title = "Temporary";
        history.observe(edited,true);
        history.observe(original,false);
        require(!history.canUndo(),"An edit reverted before completion must not create an undo step");
        for (int i=0; i<120; ++i) {
            edited.title = std::to_string(i);
            history.observe(edited,false);
        }
        int count = 0;
        while (history.undo()) ++count;
        require(count == 100,"History must be bounded to 100 edits");
        require(history.redo().has_value(),"Bounded history must retain redo");
        history.reset(original);
        require(!history.canUndo() && !history.canRedo(),"Project/media boundaries must clear both stacks");
        std::cout << "History grouping, branching, reset and capacity checks passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
