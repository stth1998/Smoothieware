/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/
#include "CustomScreen.h"
#include "libs/Kernel.h"
#include "Config.h"
#include "checksumm.h"
#include "LcdBase.h"
#include "Panel.h"
#include "ConfigValue.h"

#include <string.h>

#include <algorithm>

// sub sub items
#define enable_checksum      CHECKSUM("enable")
#define name_checksum        CHECKSUM("name")
#define command_checksum     CHECKSUM("command")

// old custom menu item
#define custom_menu_checksum CHECKSUM("custom_menu")

// multi custom menu item
#define multi_custom_menu_checksum CHECKSUM("multi_custom_menu")

// config for the new style custom menu looks that way:

// multi_custom_menu.custom_menu.enabled  true
// multi_custom_menu.custom_menu.name     Custom
//
// custom_menu.item1.enabled              true
// custom_menu.item1.name                 Custom_item1
// custom_menu.item1.command              M00
//
// multi_custom_menu.menukey.enabled      true
// multi_custom_menu.menukey.name         Custom2
//
// menukey.item1.enabled                  true
// menukey.item1.name                     Custom2_item1
// menukey.item1.command                  M00

// if the first 2 lines are omitted, only custom_menu.* -entries are detected and we have traditional behavior

using namespace std;

void CustomScreen::create_CustomScreens(std::vector<CustomScreen*> &place_menus_inside)
{
    // check for extended custom menu
    // the feature is enabled by naming the traditional standard custom menu
    string new_cm =  THEKERNEL->config->value(multi_custom_menu_checksum, custom_menu_checksum, name_checksum)->as_string();

    if (new_cm.empty())
    {
        // traditional custom menu
        CustomScreen* tmp = new CustomScreen("Custom", custom_menu_checksum);
        if (tmp->menu_items.size() > 0)
            // only add, if an item is inside
            place_menus_inside.push_back(tmp);
        else
            delete tmp;
    }
    else
    {
        // TODO i'm not sure with the order of the modules

        // new custom menu
        vector<uint16_t> modules;
        THEKERNEL->config->get_module_list( &modules, multi_custom_menu_checksum );

        // load the custom menu items
        for ( unsigned int i = 0; i < modules.size(); i++ )
        {
            if (THEKERNEL->config->value(multi_custom_menu_checksum, modules[i], enable_checksum )->as_bool())
            {
                // Get Menu entry name
                string name = THEKERNEL->config->value(multi_custom_menu_checksum, modules[i], name_checksum )->as_string();
                name = name.substr(0, 15);
                std::replace( name.begin(), name.end(), '_', ' '); // replace _ with space

                CustomScreen* tmp = new CustomScreen(strdup(name.c_str()), modules[i]);
                if (tmp->menu_items.size() > 0)
                    // only add, if an item is inside
                    place_menus_inside.push_back(tmp);
                else
                    delete tmp;
            }
        }
    }
}

CustomScreen::CustomScreen(const char *menu_name, uint16_t menu_checksum)
        : menu_name(menu_name)
{
    this->command = nullptr;

    //printf("Setting up CustomScreen\n");
    vector<uint16_t> modules;
    THEKERNEL->config->get_module_list( &modules, menu_checksum );

    // load the custom menu items
    for ( unsigned int i = 0; i < modules.size(); i++ ) {
        if (THEKERNEL->config->value(menu_checksum, modules[i], enable_checksum )->as_bool()) {
            // Get Menu entry name
            string name = THEKERNEL->config->value(menu_checksum, modules[i], name_checksum )->as_string();
            name = name.substr(0, 15);
            std::replace( name.begin(), name.end(), '_', ' '); // replace _ with space

            // Get Command
            string command = THEKERNEL->config->value(menu_checksum, modules[i], command_checksum )->as_string();
            std::replace( command.begin(), command.end(), '_', ' '); // replace _ with space
            std::replace( command.begin(), command.end(), '|', '\n'); // replace | with \n for multiple commands

            // put in menu item list
            menu_items.push_back(make_tuple(strdup(name.c_str()), strdup(command.c_str())));
            //printf("added menu %s, command %s\n", name.c_str(), command.c_str());
        }
    }
}

void CustomScreen::on_enter()
{
    THEPANEL->enter_menu_mode();
    THEPANEL->setup_menu(menu_items.size() + 1);
    this->refresh_menu();
}

void CustomScreen::on_refresh()
{
    if ( THEPANEL->menu_change() ) {
        this->refresh_menu();
    }
    if ( THEPANEL->click() ) {
        this->clicked_menu_entry(THEPANEL->get_menu_current_line());
    }
}

void CustomScreen::display_menu_line(uint16_t line)
{
    if (line == 0) {
        THEPANEL->lcd->printf("Back");
    } else {
        THEPANEL->lcd->printf(std::get<0>(menu_items[line-1]));
    }
}

void CustomScreen::clicked_menu_entry(uint16_t line)
{
    if (line == 0) {
        THEPANEL->enter_screen(this->parent);
    } else {
        command = std::get<1>(menu_items[line-1]);
    }
}

const char * CustomScreen::display_name()
{
    return this->menu_name;
}

// queuing commands needs to be done from main loop
void CustomScreen::on_main_loop()
{
    // issue command
    if (this->command == nullptr) return;
    send_command(this->command);
    this->command = nullptr;
}
