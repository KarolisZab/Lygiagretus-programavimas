#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <omp.h>

const int threadCount = 5;
const int& monitorSize = 10;
const int& dataSize = 25;
const int& filter = 1000;

const std::string& dataFile1 = "IFF06_ZabinskisK_L1_dat_1.json";
const std::string& dataFile2 = "IFF06_ZabinskisK_L1_dat_2.json";
const std::string& dataFile3 = "IFF06_ZabinskisK_L1_dat_3.json";
const std::string& outputFile = "IFF06_ZabinskisK_L1_rez.txt";

struct Car {
    std::string carModel = "";
    double consumption = 0;
    int fuelTank = 0;
};
struct CarComputed {
    Car car;
    double travelDistance;
};
class ResultMonitor {
private:
    CarComputed cars[dataSize];
    int count = 0;
public:
    double CalculateDistance(int index) {
        return cars[index].car.fuelTank / cars[index].car.consumption * 100;
    }
    double CalculateDistanceComputed(CarComputed car, int index) {
        return car.car.fuelTank / car.car.consumption * 100;
    }
    void addItemSorted(CarComputed car) {
        int current = count - 1;
        while (current >= 0 && CalculateDistance(current) > CalculateDistanceComputed(car, current)) {
            cars[current + 1] = cars[current];
            current--;
        }
        cars[current + 1] = car;
        count++;
    }
    CarComputed* getItems() {
        static CarComputed temp[dataSize];
        for (int i = 0; i < count; i++)
        {
            temp[i] = cars[i];
        }
        return temp;
    }
    int getCount() {
        return count;
    }
    bool IsEmpty() {
        return count == 0;
    }
};
ResultMonitor resultMonitor;

std::vector<Car> ReadFile(const std::string& filePath) {
    std::ifstream in(filePath);
    nlohmann::json input = nlohmann::json::parse(in);
    std::vector<Car> parsedData;
    for (auto& data : input) {
        Car dataInput{ data["carModel"],data["consumption"], data["fuelTank"] };
        parsedData.push_back(dataInput);
    }
    in.close();
    return parsedData;
}
void WriteResultsToFile(std::string path, std::vector<Car> cars, int sum, double sumD) {
    std::ofstream out(path);
    out << "Pradiniai duomenys" << std::endl;
    out << "----------------------------------------------------------------------" << std::endl;
    out << "|" << std::setw(17) << "Car Model" << std::setw(15) << "Consumption" << std::setw(15) << "Fuel Tank" << std::setw(20) << "Total Distance" << " |" << std::endl;
    out << "|--------------------------------------------------------------------|" << std::endl;
    for (int i = 0; i < cars.size(); i++) {
        if (cars[i].carModel != "") {
            double calculation = cars[i].fuelTank / cars[i].consumption * 100;
            out << "|" << i + 1 << std::setw(15) << cars[i].carModel << std::setw(15) << cars[i].consumption << std::setw(15) << cars[i].fuelTank << std::setw(20) << calculation << "  |" << std::endl;
        }
    }
    out << "-----------------------------------------------------------------------" << std::endl;
    out << std::endl;
    if (!resultMonitor.IsEmpty()) {
        out << "Rezultatai" << std::endl;
        out << "----------------------------------------------------------------------" << std::endl;
        out << "|" << std::setw(17) << "Car Model" << std::setw(15) << "Consumption" << std::setw(15) << "Fuel Tank" << std::setw(22) << "Total Distance |" << std::endl;
        out << "|--------------------------------------------------------------------|" << std::endl;
        int index = 0;
        CarComputed* sortedCar = resultMonitor.getItems();
        for (int i = 0; i < resultMonitor.getCount(); i++){
            double calculation = sortedCar[i].car.fuelTank / sortedCar[i].car.consumption * 100;
            out << "|" << index + 1 << std::setw(15) << sortedCar[i].car.carModel << std::setw(15) << sortedCar[i].car.consumption << std::setw(15) << sortedCar[i].car.fuelTank << std::setw(20) << calculation << " |" << std::endl;
            index++;
        }
        out << "----------------------------------------------------------------------" << std::endl;
        out << "Sum Int: " << std::setw(20) << sum << std::endl;
        out << "Sum Double: " << std::setw(20) << std::fixed << std::setprecision(2) << sumD << std::endl;
    }
    else {
        out << "Rezultatu nera.";
    }
    out.close();
}
int main()
{

    std::vector<Car> cars = ReadFile(dataFile1);
    for (int i = 0; i < dataSize*dataSize; i++)
    {
        Car temp;
        cars.emplace_back(temp);
    }
    omp_set_num_threads(threadCount);
    int count = 0;
    int sumFuelTank = 0;
    double sumConsumption = 0;
    #pragma omp parallel reduction(+:sumFuelTank) reduction(+:sumConsumption) reduction(+:count)
    {
        int id = omp_get_thread_num();
        while (count < dataSize)
        {
            Car car;
            if (count == 0) {
                car = cars[id];
            }
            else {
                car = cars[count * threadCount + id];
            }
            CarComputed carComp;
            carComp.car = car;
            carComp.travelDistance = carComp.car.fuelTank / carComp.car.consumption * 100;
            sumFuelTank += carComp.car.fuelTank;
            sumConsumption += carComp.car.consumption;

            #pragma omp critical
            {
                if (carComp.travelDistance > filter) {
                    resultMonitor.addItemSorted(carComp);
                }
                count++;
            }
        }
    }
    WriteResultsToFile(outputFile, cars, sumFuelTank, sumConsumption);
    return 0;
}