
<img width="1905" height="729" alt="image" src="https://github.com/user-attachments/assets/a7ae5321-9add-4b12-9971-2d3a36f8c34a" />


####$$$$$   Create new Certificates for ESP32 matter ############
Warning:  add:
1. The first thing to do is Export your custom VID/PID as environment variables to decrease chances of clerical error when 
editing your command arguments:
export VID=FFAA
export PID=FFA1

echo ${VID} 
echo ${PID}

2. Generate the CD using chip-cert. Currently the Commissioner only validates that the VID and PID match the data exposed else where 
by the device: the Basic Information Cluster, DAC and DAC origin (when it has it). You may leave the other fields unchanged:

cd /home/ntson/esp/esp-matter/connectedhomeip/connectedhomeip/
./out/host/chip-cert gen-cd \
  --key credentials/test/certification-declaration/Chip-Test-CD-Signing-Key.pem \
  --cert credentials/test/certification-declaration/Chip-Test-CD-Signing-Cert.pem \
  --out credentials/test/certification-declaration/Chip-Test-CD-${VID}-${PID}.der \
  --format-version "1" \
  --vendor-id "${VID}" \
  --product-id "${PID}" \
  --device-type-id "0x1234" \
  --certificate-id "ZIG20141ZB330001-24" \
  --security-level "0" \
  --security-info "0" \
  --version-number "9876" \
  --certification-type "0"

Look in the credentials/test/certification-declaration directory. "Chip-Test-CD-FFAA-FFA1.der"

3. Verify the CD. Make sure it contains your VID/PID (in decimal format):

./out/host/chip-cert print-cd credentials/test/certification-declaration/Chip-Test-CD-${VID}-${PID}.der

Generate a PAI and DAC
In this example we'll use Matter's own test Product Attestation Authority (PAA) certificate and signing key Chip-Test-PAA-NoVID as our root certificate. We'll use it as the root CA to generate our own PAI and DAC.

4. Generate the PAI
./out/host/chip-cert gen-att-cert --type i \
  --subject-cn "Matter Test PAI" \
  --subject-vid "${VID}" \
  --valid-from "2024-01-01 14:23:43" \
  --lifetime "4294967295" \
  --ca-key credentials/test/attestation/Chip-Test-PAA-NoVID-Key.pem \
  --ca-cert credentials/test/attestation/Chip-Test-PAA-NoVID-Cert.pem \
  --out-key credentials/test/attestation/"test-PAI-${VID}-key".pem \
  --out credentials/test/attestation/"test-PAI-${VID}-cert".pem

5. Generate the DAC using the PAI:
./out/host/chip-cert gen-att-cert --type d \
  --subject-cn "Matter Test DAC 0" \
  --subject-vid "${VID}" \
  --subject-pid "${PID}" \
  --valid-from "2024-01-01 14:23:43" \
  --lifetime "4294967295" \
  --ca-key credentials/test/attestation/"test-PAI-${VID}-key".pem \
  --ca-cert credentials/test/attestation/"test-PAI-${VID}-cert".pem \
  --out-key credentials/test/attestation/"test-DAC-${VID}-${PID}-key".pem \
  --out credentials/test/attestation/"test-DAC-${VID}-${PID}-cert".pem

6. Verify the DAC, PAI and PAA chain. If no errors appear in the output, it means that the certificate attestation chain is successfully verified:
./out/host/chip-cert validate-att-cert \
--dac credentials/test/attestation/"test-DAC-${VID}-${PID}-cert".pem \
--pai credentials/test/attestation/"test-PAI-${VID}-cert".pem \
--paa credentials/test/attestation/Chip-Test-PAA-NoVID-Cert.pem

7. Inspect your keys using openssl:
openssl ec -noout -text -in \
  credentials/test/attestation/test-DAC-${VID}-${PID}-key.pem

8. You may also use openssl to inspect your generated certificates:
 openssl x509 -noout -text -in \
  credentials/test/attestation/test-DAC-${VID}-${PID}-cert.pem

Note: A similar process could be used for generating a self-signed PAA, but doing so is not necessary.

Instead, what we've done here is to use an existing self-signed development PAA that doesn't include VID information.

For more examples of generating a CD, look at credentials/test/gen-test-cds.sh And for more examples of generating a PAA, PAI, and DAC, see credentials/test/gen-test-attestation-certs.sh




              #####################$ "Hard coding the DAC into the app" ##################$


In this section we will hard code the DAC certs into the Matter

1. Use a script to generate template code
Save the following script into a file called generate-embeddable-certs.sh

cd /home/ntson/esp/esp-matter/connectedhomeip/connectedhomeip/credentials/test   this is "generate-embeddable-certs.sh"

------------        chmod +x generate-embeddable-certs.sh ######### Cấp quyền thực thi cho script
-----------       ./generate-embeddable-certs.sh          ########### Chạy script tao ra noi dung

Edit the ExampleDACs.cpp file: sau do copy noi dung vao cuoi file: src/credentials/examples/ExampleDACs.cpp

3. Replace the CD (certification-declaration)
Extract a text representation of the contents of your CD file using xxd:

cd /home/ntson/esp/esp-matter/connectedhomeip/connectedhomeip/credentials/test/certification-declaration 
#########        xxd -i Chip-Test-CD-FFAA-FFA1.der ############ 

Chay tao ra ket qua du lieu add vao src/credentials/examples/DeviceAttestationCredsExample.cpp







////////////////////////     Generate PKI credentials for ESP32 matter devices in esp_secure_cert partition   ///////////////////////////////

In this section, we will generate PKI credentials for ESP32 matter devices and store them in esp_secure_cert partition

1. Change format for the certificates and key (.pem to .der format).
    Convert DAC key from .pem to .der format.:

cd /home/ntson/esp/esp-matter/connectedhomeip/connectedhomeip/credentials/test/attestation
openssl ec -in "test-DAC-${VID}-${PID}-key".pem -out "test-DAC-${VID}-${PID}-key".der -inform pem -outform der

     Convert DAC and PAI cert from .pem to .der format:

openssl x509 -in "test-DAC-${VID}-${PID}-cert".pem -out "test-DAC-${VID}-${PID}-cert".der -inform pem -outform der
openssl x509 -in "test-PAI-${VID}-cert".pem -out "test-PAI-${VID}-cert".der -inform pem -outform der


2. Generate and flash the secure partition The following command generates the secure cert partition and flashes it to the connected device. Additionally, it preserves the generated partition on the host, allowing it to be flashed later if the entire flash is erased.


configure_esp_secure_cert.py --private-key "test-DAC-${VID}-${PID}-key".der \
    --device-cert "test-DAC-${VID}-${PID}-cert".der \
    --ca-cert "test-PAI-${VID}-cert".der \
    --target_chip esp32c6 \
    --keep_ds_data_on_host \
    --port /dev/ttyACM0 \
    --priv_key_algo ECDSA 256
    
ESP32H2

configure_esp_secure_cert.py --private-key "test-DAC-${VID}-${PID}-key".der \
    --priv_key_algo ECDSA 256 --efuse_key_id 0 --configure_ds \
    --device-cert "test-DAC-${VID}-${PID}-cert".der \
    --ca-cert "test-PAI-${VID}-cert".der \
    --target_chip esp32h2 \
    --keep_ds_data_on_host \
    --port /dev/ttyACM0 

configure_esp_secure_cert.py --private-key "test-DAC-${VID}-${PID}-key".pem \
    --priv_key_algo ECDSA 256 --efuse_key_id 0 --configure_ds \
    --device-cert "test-DAC-${VID}-${PID}-cert".pem \
    --ca-cert "test-PAI-${VID}-cert".pem \
    --target_chip esp32h2 \
    --keep_ds_data_on_host \
    --port /dev/ttyACM0 
    
You should notice that a new esp_secure_cert_data folder is created with the esp secure cert file.

//////////////////////////////////                Configure the esp Matter light application to use the secure certificates //////////////
# Disable the DS Peripheral support (ESP32-H2 Yes)
CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL=n

# Use DAC Provider implementation which reads attestation data from secure cert partition
CONFIG_SEC_CERT_DAC_PROVIDER=y

# Enable some options which reads CD and other basic info from the factory partition
CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER=y
CONFIG_ENABLE_ESP32_DEVICE_INSTANCE_INFO_PROVIDER=y
CONFIG_FACTORY_COMMISSIONABLE_DATA_PROVIDER=y
CONFIG_FACTORY_DEVICE_INSTANCE_INFO_PROVIDER=y


//////////////////////////////////// Factory Partition    //////////////////////////
We will not set up the Factory partition which contains basic information like VID, PID, etc, and CD.

Sao chép và đổi tên tệp chứng nhận hiện có (cd /home/ntson/esp/esp-matter/connectedhomeip/connectedhomeip/credentials/test/certification-declaration)
cp Chip-Test-CD-FFAA-FFA1.der  ~/esp/esp-matter/tools/mfg_tool/Chip-Test-CD-FFAA-FFA1.der 

Tạo một tệp chứng nhận mới lưu dưới tên Chip-Test-CD-FFAA-FFA1.der trong thư mục /home/ntson/esp/esp-matter/tools/mfg_tool/.

1. Export the dependent tools path

cd /home/ntson/esp/esp-matter/tools/mfg_tool
export PATH=$PATH:$PWD/../../connectedhomeip/connectedhomeip/out/host

Tạo NVS binary bằng mfg_tool:
Generate the factory partition, please use the APPROPRIATE values for -v (Vendor Id), -p (Product Id), and -cd (Certification Declaration).

esp-matter-mfg-tool --passcode 20202507 \
    --discriminator 2002 \
    -cd Chip-Test-CD-FFAA-FFA1.der \
    -v 0xFFAA --vendor-name NTS-20202507 \
    -p 0xFFA1 --product-name ĐATN \
    --hw-ver 1 --hw-ver-str DevKit\
    --target esp32h2 --efuse-key-id 0


Few important output lines are mentioned below. Please take a note of onboarding codes, these can be used for commissioning the device.

Flashing firmware, secure cert and factory partition


Bước 1: Flash chứng chỉ bảo mật (esp_secure_cert):
Tìm kiếm file esp_secure_cert.bin bằng lệnh: find ~/ -name esp_secure_cert.bin

esptool.py -p /dev/ttyACM0 write_flash 0xd000 /home/ntson/esp/esp-matter/connectedhomeip/connectedhomeip/credentials/test/attestation/esp_secure_cert_data/esp_secure_cert.bin

Tìm file NVS binary sau khi chạy lại mfg_tool Sau khi chạy lệnh trên, kiểm tra xem file NVS binary có được tạo hay không:
                find ~/esp/esp-matter/tools/mfg_tool/out/ -name "*.bin"

Flash file binary vào ESP32:

esptool.py -p /dev/ttyACM0 write_flash 0x10000 /home/ntson/esp/esp-matter/tools/mfg_tool/out/ffaa_ffa1/7ee2f45f-e322-4d53-87cf-433f104e20e9/7ee2f45f-e322-4d53-87cf-433f104e20e9-partition.bin

Cleaning Up
You should stop the switch-app process by using Ctrl-] in the first esp32 monitor window, the light-app process by using Ctrl-] in the second esp32 monitor window and then run idf erase flash.

It also a great habit to clean up the temporary files after you finish testing by using this command:

rm -fr /tmp/chip_*
