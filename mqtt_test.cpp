
#include <iostream>
#include <thread>
#include <chrono>
#include <mqtt/async_client.h>

const std::string BROKER_ADDRESS = "tcp://localhost:1883";

const std::string CLIENT_ID = "smart_home_cpp_test";

const std::string TOPIC = "home/test";

const std::string PAYLOAD = "23.5 from C++";

int main() {
    try {
        std::cout << "Создаю клиента..." << std::endl;
        mqtt::async_client client(BROKER_ADDRESS, CLIENT_ID);

        auto options = mqtt::connect_options_builder()
            .automatic_reconnect(true)
            .finalize();

        std::cout << "Подключение к " << BROKER_ADDRESS << "..." << std::endl;
        client.connect(options)->wait();
        std::cout << "[OK] Подключено к брокеру" << std::endl;

        std::cout << "Публикую в '" << TOPIC << "': " << PAYLOAD << std::endl;

        auto pubmsg = mqtt::make_message(TOPIC, PAYLOAD);
        pubmsg->set_qos(1);

        client.publish(pubmsg)->wait();

        std::cout << "[OK] Сообщение отправлено" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));

        client.disconnect()->wait();
        std::cout << "[OK] Программа завершена успешно" << std::endl;
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[ERROR] MQTT: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}