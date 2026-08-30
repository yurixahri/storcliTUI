#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <memory>
#include "cpptui.hpp" // Your single-header library
#include "nlohmann/json.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h> // Windows native APIs
#else
    #include <unistd.h>  // UNIX standard APIs (Linux/macOS)
#endif

using namespace cpptui;
using namespace std::chrono_literals;
using json = nlohmann::json;

int controller_index = -1;

static std::jthread bg_job;
void controller_dashboard_scene(std::shared_ptr<Vertical> root, App &app);
void controller_select_scene(std::shared_ptr<Vertical> root, App &app);

bool is_run_as_admin() {
#if defined(_WIN32) || defined(_WIN64)
    // ==========================================
    // WINDOWS ELEVATION CHECK
    // ==========================================
    BOOL fIsElevated = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize = sizeof(TOKEN_ELEVATION);

        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            fIsElevated = elevation.TokenIsElevated;
        }
    }

    if (hToken) {
        CloseHandle(hToken);
    }

    return fIsElevated == TRUE;

#else
    // ==========================================
    // LINUX / MACOS ROOT CHECK
    // ==========================================
    // geteuid() returns the Effective User ID of the process.
    // Root/sudo users always have an Effective User ID of 0.
    return (geteuid() == 0);
#endif
}

std::string storcli_output(const std::string& args) {
    std::array<char, 100000> buffer;
    std::string output = "";
    
    // Combine the global binary name with your query flags
    std::string command = "storcli " + args;

    // Open a read-only stream directly from the Windows command interpreter
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        return "Error: Could not invoke the storcli process pipeline.";
    }

    // Capture the output text chunk-by-chunk until the process ends
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }

    _pclose(pipe);
    
    if (output.empty()) {
        return "Warning: storcli executed but returned an empty response stream.";
    }

    return output;
}

bool is_storcli_exist() {
    std::array<char, 128> buffer;
    std::string result = "";
    
    #if defined(_WIN32) || defined(_WIN64)
    FILE* pipe = _popen("where storcli", "r");
    #else
    FILE* pipe = _popen("which storcli", "r");
    #endif
    
    if (!pipe) {
        return false;
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int exit_code = _pclose(pipe);

    if (exit_code == 0 && !result.empty()) {
        std::cout << "[Found] Global executable mapped via environment path:\n" 
                  << result; // Prints the exact location found by Windows
        return true;
    }

    std::cerr << "[Error] 'storcli' is not recognized as a global command.\n";
    return false;
}

void controller_dashboard_scene(std::shared_ptr<Vertical> root, App &app){
    root->clear_children();

    auto info_panel_border = std::make_shared<Border>(BorderStyle::Rounded);
    info_panel_border->set_title("Controller Info");
    info_panel_border->set_color(Color(227, 242, 253));
    

    auto vd_panel_border = std::make_shared<Border>(BorderStyle::Rounded);
    vd_panel_border->set_title("Virtual Drives");
    vd_panel_border->set_color(Color(227, 242, 253));

    auto drive_panel_border = std::make_shared<Border>(BorderStyle::Rounded);
    drive_panel_border->set_title("Drives");
    drive_panel_border->set_color(Color(227, 242, 253));

    auto cv_panel_border = std::make_shared<Border>(BorderStyle::Rounded);
    cv_panel_border->set_title("Cachevault");
    cv_panel_border->set_color(Color(227, 242, 253));

    auto help = std::make_shared<Label>("q: Quit, b: Back");
    help->bg_color = Color(127, 142, 153);
    help->fg_color = Color(35, 60, 104);

    auto cv_info_layout = std::make_shared<Horizontal>();
    cv_info_layout->add(info_panel_border);
    cv_info_layout->add(std::make_shared<HorizontalSpacer>(1));
    cv_info_layout->add(cv_panel_border);
    root->add(cv_info_layout);
    root->add(std::make_shared<VerticalSpacer>(1));
    root->add(vd_panel_border);
    root->add(std::make_shared<VerticalSpacer>(1));
    root->add(drive_panel_border);
    root->add(help);
    help->set_focus(true);

    bg_job = std::jthread([info_panel_border, vd_panel_border, drive_panel_border, cv_panel_border, &app](std::stop_token token){
        while(!token.stop_requested()){
            if (token.stop_requested()) break;
            std::string cmd = "/c"+ std::to_string(controller_index) +" show J";
            
            json data = json::parse( storcli_output(cmd) );
            json response_data = data["Controllers"][0]["Response Data"];
            if (token.stop_requested()) break;
            
            
            info_panel_border->clear_children();
            vd_panel_border->clear_children();
            drive_panel_border->clear_children();
            cv_panel_border->clear_children();
            
            if (token.stop_requested()) break;
            auto info_panel_layout = std::make_shared<Vertical>();
            auto product_name = std::make_shared<Label>( "Product Name: " + response_data["Product Name"].dump() );
            auto serial_number = std::make_shared<Label>( "Serial Number: " + response_data["Serial Number"].dump() );
            auto sas_address = std::make_shared<Label>( "SAS Address: " + response_data["SAS Address"].dump() );
            auto pci_address = std::make_shared<Label>( "PCI Address: " + response_data["PCI Address"].dump() );
            auto controller_time = std::make_shared<Label>( "Controller Time: " + response_data["Controller Time"].dump() );
            auto fw_package_build = std::make_shared<Label>( "FW Package Build: " + response_data["FW Package Build"].dump() );
            auto bios = std::make_shared<Label>( "BIOS Version: " + response_data["BIOS Version"].dump() );
            auto fw = std::make_shared<Label>( "FW Version: " + response_data["FW Version"].dump() );
            info_panel_layout->add(product_name);
            info_panel_layout->add(serial_number);
            info_panel_layout->add(sas_address);
            info_panel_layout->add(pci_address);
            info_panel_layout->add(controller_time);
            info_panel_layout->add(fw_package_build);
            info_panel_layout->add(bios);
            info_panel_layout->add(fw);
            
            auto vd_panel_layout = std::make_shared<Horizontal>();
            for (const auto &vd : response_data["VD LIST"]){
                auto vd_border = std::make_shared<Border>(BorderStyle::ASCII);
                vd_border->set_title( vd["DG/VD"].dump() );
                vd_border->set_color(Color(127, 142, 153));
                
                auto vd_layout = std::make_shared<Vertical>();
                auto vd_type = std::make_shared<Label>( "Type: " + vd["TYPE"].dump() );
                auto vd_state = std::make_shared<Label>( "State: " + vd["State"].dump() );
                auto vd_access = std::make_shared<Label>( "Access: " + vd["Access"].dump() );
                auto vd_consist = std::make_shared<Label>( "Consist: " + vd["Consist"].dump() );
                auto vd_cache = std::make_shared<Label>( "Cache: " + vd["Cache"].dump() );
                auto vd_size = std::make_shared<Label>( "Size: " + vd["Size"].dump() );
                vd_layout->add(vd_type);
                vd_layout->add(vd_state);
                vd_layout->add(vd_access);
                vd_layout->add(vd_consist);
                vd_layout->add(vd_cache);
                vd_layout->add(vd_size);
                vd_border->add(vd_layout);
                vd_panel_layout->add(vd_border);
            }
            
            auto drive_panel_layout = std::make_shared<Horizontal>();
            for (const auto &drive : response_data["PD LIST"]){
                auto drive_border = std::make_shared<Border>(BorderStyle::ASCII);
                drive_border->set_title( drive["EID:Slt"].dump() );
                drive_border->set_color(Color(127, 142, 153));
                
                auto drive_layout = std::make_shared<Vertical>();
                auto drive_dg = std::make_shared<Label>( "DG: " + drive["DG"].dump() );
                auto drive_state = std::make_shared<Label>( "State: " + drive["State"].dump() );
                auto drive_size = std::make_shared<Label>( "Size: " + drive["Size"].dump() );
                auto drive_model = std::make_shared<Label>( "Model: " + drive["Model"].dump() );
                
                drive_layout->add(drive_dg);
                drive_layout->add(drive_state);
                drive_layout->add(drive_size);
                drive_layout->add(drive_model);
                drive_border->add(drive_layout);
                drive_panel_layout->add(drive_border);
            }

            auto cv_panel_layout = std::make_shared<Vertical>();
            auto cv_model = std::make_shared<Label>( "Model: " + response_data["Cachevault_Info"][0]["Model"].dump() );
            auto cv_state = std::make_shared<Label>( "State: " + response_data["Cachevault_Info"][0]["State"].dump() );
            auto cv_temp = std::make_shared<Label>( "Temp: " + response_data["Cachevault_Info"][0]["Temp"].dump() );
            auto cv_mfg_date = std::make_shared<Label>( "MfgDate: " + response_data["Cachevault_Info"][0]["MfgDate"].dump() );
            auto cv_next_learn = std::make_shared<Label>( "Next Learn: " + response_data["Cachevault_Info"][0]["Next Learn"].dump() );
            cv_panel_layout->add(cv_model);
            cv_panel_layout->add(cv_state);
            cv_panel_layout->add(cv_temp);
            cv_panel_layout->add(cv_mfg_date);
            cv_panel_layout->add(cv_next_learn);

            info_panel_border->add(info_panel_layout);
            vd_panel_border->add(vd_panel_layout);
            drive_panel_border->add(drive_panel_layout);
            cv_panel_border->add(cv_panel_layout);

            info_panel_border->min_height = 10;
            vd_panel_border->min_height = 8;
            drive_panel_border->min_height = 6;
            cv_panel_border->min_height = 10;
            app.update();
            for (int i = 0; i < 50; ++i) {
                if (token.stop_requested()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
    
    app.register_key('b', [&app, root](){
        app.unregister_key('b');
        bg_job = std::jthread();
        controller_index = -1;
        controller_select_scene(root, app);
    });
}

void controller_select_scene(std::shared_ptr<Vertical> root, App &app){
    root->clear_children();
    auto container = std::make_shared<Horizontal>();
    auto bottom_status = std::make_shared<Label>("Selecting:");
    auto help = std::make_shared<Label>("q: Quit, b: Back");

    root->add(container);
    root->add(std::make_shared<VerticalSpacer>());

    auto bottom_panel = std::make_shared<Horizontal>();
    bottom_status->bg_color = Color(227, 242, 253);
    bottom_status->fg_color = Color(15, 40, 84);

    help->bg_color = Color(127, 142, 153);
    help->fg_color = Color(35, 60, 104);
    root->add(help);
    root->add(bottom_status);
    
    std::string cmd = "show J";
    json data = json::parse( storcli_output(cmd) );
    json response_data = data["Controllers"][0]["Response Data"];

    for (const auto &controller : response_data["System Overview"]){
        auto btn_container = std::make_shared<Vertical>();
        btn_container->fixed_height = 2; 

        std::string ctrl_id = "Controller " + controller["Ctl"].dump();
        size_t ctrl_index = controller["Ctl"].get<int>();
        std::string ctrl_model = "Model: " + controller["Model"].dump();

        auto btn = std::make_shared<Button>(ctrl_id, [ctrl_index, root, &app](){
            controller_index = ctrl_index;
            controller_dashboard_scene(root, app);
        });
        
        btn->on_custom_focus = [ctrl_id, bottom_status](bool is_focused) {
            if (is_focused){
                bottom_status->set_text("Selecting: " + ctrl_id);
            }else{
                bottom_status->set_text("Selecting:");
            }
        };
        
        auto label = std::make_shared<Label>(ctrl_model);

        auto aligned_btn = std::make_shared<Align>(); 
        aligned_btn->add(btn);
        auto aligned_label = std::make_shared<Align>();
        label->fixed_width = ctrl_model.length();
        aligned_label->add(label);

        btn_container->add(aligned_btn);
        btn_container->add(aligned_label);

        size_t max_width = std::max<size_t>(ctrl_id.length(), ctrl_model.length());
        btn_container->fixed_width = max_width;

        auto centered_block = std::make_shared<Align>();
        centered_block->add(btn_container);
        auto border_wrapper = std::make_shared<Border>(BorderStyle::ASCII);
        border_wrapper->fixed_width = btn_container->fixed_width + 4;
        border_wrapper->fixed_height = btn_container->fixed_height + 2;
        border_wrapper->add(centered_block);
        container->add(border_wrapper);
    }
}

int main() {

    if (!is_run_as_admin()){
        std::cout << "Please run this app as admin/sudo.\n";
        return 1;
    }
    std::cout << "Checking environment variables...\n";
    
    if (is_storcli_exist()) {
        std::cout << "System ready.\n";
    } else {
        std::cout << "Initialization aborted.\n";
        return 1;
    }
    // Sleep(1000);

    App app;
    Theme::set_theme(Theme::TokyoNight()); 
    auto root_layout = std::make_shared<Vertical>();
    
    controller_select_scene(root_layout, app);
    app.register_exit_key('q');
    app.run(root_layout);

    return 0;
}
