#include <FES/Core/Stimulator.hpp>
#include <deque>
#include <Mahi/Util.hpp>
#include <mutex>
#include <thread>
#include <Mahi/Gui.hpp>

namespace fes{
    class Visualizer : public mahi::gui::Application{
    private:
        ImGuiInputTextFlags enabled_flags = 0;
        std::thread viz_thread;
        Stimulator *stimulator;
        std::vector<int> amp;
        std::vector<int> pw;
        std::vector<int> max_amp;
        std::vector<int> max_pw;
        size_t num_channels;
        std::deque<bool> enabled = {false, false, false, false, false, false, false, false};
        ImGui::PlotInterface plot_interface;
        std::vector<ImGui::PlotItem> items;
        mahi::util::Clock elapse_clock;
        std::vector<Channel> channels;
        bool open = true;
        std::mutex mtx;
        std::vector<ImVec4> color = {ImVec4(0.0f/255.0f,   218.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f),
                                     ImVec4(20.0f/255.0f,  220.0f/255.0f, 0.0f/255.0f,   255.0f/255.0f),
                                     ImVec4(255.0f/255.0f, 120.0f/255.0f, 0.0f/255.0f,   255.0f/255.0f),
                                     ImVec4(255.0f/255.0f, 0.0f/255.0f,   0.0f/255.0f,   255.0f/255.0f),
                                     ImVec4(150.0f/255.0f, 0.0f/255.0f,   255.0f/255.0f, 255.0f/255.0f),
                                     ImVec4(0.0f/255.0f,   30.0f/255.0f,  255.0f/255.0f, 255.0f/255.0f),
                                     ImVec4(255.0f/255.0f, 0.0f/255.0f,   255.0f/255.0f, 255.0f/255.0f),
                                     ImVec4(126.0f/255.0f, 126.0f/255.0f, 126.0f/255.0f, 255.0f/255.0f),};
    public:
        Visualizer(Stimulator *stimulator_);
        ~Visualizer();

        void roll_point(ImGui::PlotItem& item, mahi::util::Time t, int pos);

        void update();

    };
}
