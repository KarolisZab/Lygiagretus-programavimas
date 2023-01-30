#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <iomanip>

using namespace std;

const int& threadCount = 5;
const int& monitorSize = 10;
const int& dataSize = 25;
const int& filter = 1000;

mutex monitorLock;
mutex sortedMonitorLock;

condition_variable cv;
condition_variable cvs;

const string& dataFile1 = "IFF96_JuskysT_L1_dat_1.json";
const string& dataFile2 = "IFF96_JuskysT_L1_dat_2.json";
const string& dataFile3 = "IFF96_JuskysT_L1_dat_3.json";
const string& outputFile = "IFF96_JuskysT_L1_rez.txt";

struct Car {
	string carModel;
	double consumption;
	int fuelTank;
};

struct CarComputed {
	Car car;
	double travelDistance;
};

class DataMonitor {
private:
	Car cars[monitorSize];
	bool Finished = false;
	int count = 0;
public:
	void addItem(Car car, bool finished) {
		unique_lock<mutex> lock(monitorLock);
		while (count == monitorSize) {
			cv.wait(lock);
		}
		cars[count] = car;
		count++;
		Finished = finished;
		cv.notify_all();
	}
	Car getItem() {
		unique_lock<mutex> lock(monitorLock);
		while (count == 0) {
			cv.wait(lock);
		}
		count--;
		Car output = cars[count];
		cv.notify_all();
		return output;
	}
	bool get_finished() {
		return Finished && count == 0;
	}
};

DataMonitor dataMonitor;

class ResultMonitor {
private:
	CarComputed cars[dataSize];
	bool Finished = false;
	int count = 0;
public:
	double CalculateDistance(int index) {
		return cars[index].car.fuelTank / cars[index].car.consumption * 100;
	}
	double CalculateDistanceComputed(CarComputed car, int index) {
		return car.car.fuelTank / car.car.consumption * 100;
	}
	void addItemSorted(CarComputed car, bool finish) {
		unique_lock<mutex> lock(sortedMonitorLock);
		while (count == dataSize) {
			cv.wait(lock);
		}
		int i;
		for (i = count - 1; i >= 0 && CalculateDistance(i) > CalculateDistanceComputed(car, i); i--) {
			cars[i + 1] = cars[i];
		}
		cars[i + 1] = car;
		count++;
		Finished = finish;
		cv.notify_all();
	}
	CarComputed getItem() {
		unique_lock<mutex> lock(sortedMonitorLock);
		while (count == 0) {
			cv.wait(lock);
		}
		count--;
		CarComputed output = cars[count];
		cv.notify_all();
		return output;
	}
	bool IsEmpty() {
		return count == 0;
	}
};

ResultMonitor resultMonitor;

vector<Car> ReadFile(const string& filePath) {
	ifstream in(filePath);
	nlohmann::json input = nlohmann::json::parse(in);
	vector<Car> parsedData;
	for (auto& data : input) {
		Car dataInput{ data["carModel"],data["consumption"], data["fuelTank"] };
		parsedData.push_back(dataInput);
	}
	in.close();
	return parsedData;
}

void WriteResultsToFile(string path, vector<Car> cars) {
	ofstream out(path);
	out << "Pradiniai duomenys" << endl;
	out << "----------------------------------------------------------------------" << endl;
	out << "|" << setw(17) << "Car Model" << setw(15) << "Consumption" << setw(15) << "Fuel Tank" << setw(20) << "Total Distance"<< " |" << endl;
	out << "|--------------------------------------------------------------------|" << endl;
	for (int i = 0; i < cars.size(); i++) {
		double calculation = cars[i].fuelTank / cars[i].consumption * 100;
		out << "|" << i + 1 << setw(15) << cars[i].carModel << setw(15) << cars[i].consumption << setw(15) << cars[i].fuelTank << setw(20) << calculation <<"  |" << endl;
	}
	out << "-----------------------------------------------------------------------" << endl;
	out << endl;
	if (!resultMonitor.IsEmpty()) {
		out << "Rezultatai" << endl;
		out << "----------------------------------------------------------------------" << endl;
		out << "|" << setw(17) << "Car Model" << setw(15) << "Consumption" << setw(15) << "Fuel Tank" << setw(22) << "Total Distance |" << endl;
		out << "|--------------------------------------------------------------------|" << endl;
		int index = 0;
		while (!resultMonitor.IsEmpty()) {
			CarComputed sortedCar = resultMonitor.getItem();
			double calculation = sortedCar.car.fuelTank / sortedCar.car.consumption * 100;
			out << "|" << index + 1 << setw(15) << sortedCar.car.carModel << setw(15) << sortedCar.car.consumption << setw(15) << sortedCar.car.fuelTank << setw(20) << calculation << " |" << endl;
			index++;
		}
		out << "----------------------------------------------------------------------" << endl;
	}
	else {
		out << "Rezultatu nera.";
	}
	out.close();
}

void WorkerThreadFunction() {
	while (!dataMonitor.get_finished()) {
		Car car = dataMonitor.getItem();
		CarComputed carComp;
		carComp.car = car;
		carComp.travelDistance = car.fuelTank / car.consumption * 100;
		this_thread::sleep_for(chrono::milliseconds(500));
		if (carComp.travelDistance > filter) {
			resultMonitor.addItemSorted(carComp, false);
		}
	}
}

int main() {
	vector<Car> cars = ReadFile(dataFile2);
	thread threads[threadCount];
	for (int i = 0; i < threadCount; i++)
	{
		threads[i] = thread(WorkerThreadFunction);
	}

	bool finished = false;
	for (int i = 0; i < cars.size(); i++)
	{
		dataMonitor.addItem(cars[i], finished);

		if (i == cars.size() - 2) {
			finished = true;
		}
	}
	for (int i = 0; i < threadCount; i++)
	{
		threads[i].join();
	}
	WriteResultsToFile(outputFile, cars);
	return 0;
}
