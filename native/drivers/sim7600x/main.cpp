/**
 *  @filename   :   main.cpp
 *  @brief      :   Implements for sim7600c 4g hat raspberry pi demo
 *  @author     :   Kaloha from Waveshare
 *
 *  Copyright (C) Waveshare     April 27 2018
 *  http://www.waveshare.com / http://www.waveshare.net
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common/base.h"

#include "drivers/sim7600x/arduPi.h"
#include "drivers/sim7600x/sim7x00.h"

// Pin definition
int POWERKEY = 6;

constexpr char Message[] =
    "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. "
    "Aenean massa. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur "
    "ridiculus mus. Donec quam felis, ultricies nec, pellentesque eu, pretium quis, sem. Nulla "
    "consequat massa quis enim. Donec pede justo, fringilla vel, aliquet nec, vulputate eget, "
    "arcu. In enim justo, rhoncus ut, imperdiet a, venenatis vitae, justo. Nullam dictum felis eu "
    "pede mollis pretium. Integer tincidunt. Cras dapibus. Vivamus elementum semper nisi. Aenean "
    "vulputate eleifend tellus. Aenean leo ligula, porttitor eu, consequat vitae, eleifend ac, "
    "enim. Aliquam lorem ante, dapibus in, viverra quis, feugiat a, tellus. Phasellus viverra "
    "nulla ut metus varius laoreet. Quisque rutrum. Aenean imperdiet. Etiam ultricies nisi vel "
    "augue. Curabitur ullamcorper ultricies nisi. Nam eget dui. Etiam rhoncus. Maecenas tempus, "
    "tellus eget condimentum rhoncus, sem quam semper libero, sit amet adipiscing sem neque sed "
    "ipsum. Nam quam nunc, blandit vel, luctus pulvinar, hendrerit id, lorem.";

/***********************Phone calls**********************/
char phone_number[] = "***";

/***********************FTP upload and download***************************/
char ftp_user_name[] = "**********";
char ftp_user_password[] = "**********";
char ftp_server[] = "www.waveshare.net";
char download_file_name[] = "index3.htm";
char upload_file_name[] = "index2.htm";

/*********************TCP and UDP**********************/
char APN[] = "mmsbouygtel.com";
char aux_string[60];
char ServerIP[] = "118.190.93.84";
char Port[] = "2317";

int main(int argc, char *argp[]) {
    sim7600.PowerOn(POWERKEY);

    /*************Chip information *************/
    sim7600.sendATcommand("AT+CGMI", 500);
    sim7600.sendATcommand("AT+CGMM", 500);
    sim7600.sendATcommand("AT+CPIN?", 500);

    //  sim7600.PhoneCall(phone_number);
    //  sim7600.SendingShortMessage(phone_number, Message);
    //  sim7600.ReceivingShortMessage();
    //  sim7600.ConfigureFTP(ftp_server,ftp_user_name,ftp_user_password);
    //  sim7600.UploadToFTP(upload_file_name);
    //  sim7600.DownloadFromFTP(download_file_name);
    //  sim7600.GPSPositioning();

    /*************Network environment checking*************/
    sim7600.sendATcommand("AT+CSQ", 500);
    sim7600.sendATcommand("AT+CREG?", "+CREG: 0,1", 500);
    Serial.println("AT+CPSI?");
    sim7600.sendATcommand("AT+CGREG?", "+CGREG: 0,1", 500);

    /*************PDP Context Enable/Disable*************/
    /****************************************************
    PDP context identifier setting

    AT+CGDCONT = <cid>,<PDP_type>,<APN>...
    <cid>:minimum value =1
    <PDP_type>: IP、PPP、IPV6、IPV4V6
    <APN>：(Access Point Name)
    ****************************************************/
    sprintf(aux_string, "AT+CGSOCKCONT=1,\"IP\",\"%s\"", APN);
    sim7600.sendATcommand(aux_string, "OK", 1000);        // APN setting
    sim7600.sendATcommand("AT+CSOCKSETPN=1", "OK", 1000); // PDP profile number setting value:1~16

    /*********************Enable PDP context******************/
    sim7600.sendATcommand("AT+CIPMODE=0", "OK", 1000);        // command mode,default:0
    sim7600.sendATcommand("AT+NETOPEN", "+NETOPEN: 0", 5000); // Open network
    sim7600.sendATcommand("AT+IPADDR", "+IPADDR:", 1000);     // Return IP address

    /*********************TCP client in command mode******************/
    ::snprintf(aux_string, sizeof(aux_string), "AT+CIPOPEN=0,\"%s\",\"%s\",%s", "TCP", ServerIP,
               Port);
    sim7600.sendATcommand(aux_string, "+CIPOPEN: 0,0", 5000); // Setting tcp mode, server ip and
                                                              // port
    // Sending Message to server.
    sim7600.sendRequest(Message, sizeof(Message));

    printf("\n");

    sim7600.sendATcommand("AT+CIPCLOSE=0", "+CIPCLOSE: 0,0", 15000); // Close by local
    sim7600.sendATcommand("AT+NETCLOSE", "+NETCLOSE: 0", 1000);      // Close network

    return (0);
}
