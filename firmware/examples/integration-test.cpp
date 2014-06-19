/**
 * Copyright 2014  Matthew McGowan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

 /**
  * Runs eeprom device tests against the address-erasable eeprom emulation.
  */

#include "flashee-eeprom/integration-tests.h"

FlashDeviceTest* test = NULL;

using namespace Flashee;

void setup()
{
    Serial.begin(9600);
}


void loop()
{
    static bool run = false;

    if (Serial.available()) {
        char c = Serial.read();

        if (c=='t') {
            if (!run) {
                Serial.println("Running tests");
                run = true;
                // allocate the maximum size possible. At present this is 256 pages, keeping 2 free pages.
                test = new FlashDeviceTest(Flashee::createAddressErase(0, 4096*256, 2));
                if (test==NULL)
                    Serial.println("Cannot allocate test harness");
            }
        }
    }
    if (run) {
        Test::run();
    }
}

