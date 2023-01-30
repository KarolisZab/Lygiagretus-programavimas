#include "nlohmann/json.hpp"
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
const int& dataSize = 30;
const int& filter = 200;

mutex monitorLock;
mutex sortedMonitorLock;

condition_variable cv;

const string& dataFile1 = "IFF06_ZabinskisK_L1_dat_1.json";
const string& dataFile2 = "IFF06_ZabinskisK_L1_dat_2.json";
const string& dataFile3 = "IFF06_ZabinskisK_L1_dat_3.json";
const string& outputFile = "IFF06_ZabinskisK_L1_rez.txt";

struct Product {
	string name;
	int quantity;
	double price;
};

struct ProductComputed {
	Product product;
	double totalPrice;
	void changeTotalPrice() {
		std::cout << "Calculating total price of product: \"" << product.name << "\"" << std::endl;
		totalPrice = product.quantity * product.price;
	}
};

class DataMonitor {
private:
	Product products[monitorSize];
	bool Finished = false;
	int count = 0;

public:
	void addItem(Product product, bool finished) {
		unique_lock<mutex> lock(monitorLock);

		// reikia, nes stebi, kada addItem pasieks monitorSize dydi, ir tada sustos
		while (count == monitorSize)
		{
			cv.wait(lock);
		}

		products[count] = product;
		count++;
		Finished = finished;
		// pranes getItem, kad galima imti duomenis
		cv.notify_all();
	}

	Product getItem()
	{
		unique_lock<mutex> lock(monitorLock);

		// reikia, nes stebi, kada getItem pasieks 0 ir uzsirakins
		while (count == 0)
		{
			cv.wait(lock);
		}

		count--;
		Product output = products[count];
		//informuoja, kad addItem gali toliau deti duomenis
		cv.notify_all();
		return output;
	}

	bool get_finished()
	{
		return Finished && count == 0;
	}
};

//inicijuojamas duomenu monitorius
DataMonitor dataMonitor;

class ResultMonitor {
private:
	ProductComputed products[dataSize];
	bool Finished = false;
	int count = 0;

public:
	double CalculateTotalPrice(int index)
	{
		return products[index].product.quantity * products[index].product.price;
	}

	double CalculateTotalPriceComputed(ProductComputed product, int index)
	{
		return product.product.quantity * product.product.price;
	}

	void addItemSorted(ProductComputed product, bool finish)
	{
		unique_lock<mutex> lock(sortedMonitorLock); // M. Vasil sake, kad nei while, nei cv sitam nereikia
		

		//sorte tai ten i nauja lista dedam duomenis ir kiekvienu kartu kai ideda patikrina kurioje vietoje ateinantis 
		//                                                                                naujas produktas pagal calculated total price randasi
		
		//is pradziu buna tuscias, tai tas foras net neprasisuka, iskart products[i + 1] = product buna. tada kai antras ateina, eina per lista kuris yra vieno elemento, 
		//patikrina ar ateinanti reiksme didesne uz esancia liste jei jo tada pastumia ta elementa vienu auksciau ir y jo vieta ydeda nauja reiksme. ir taip toliau iki 10
		int i;
		for (i = count - 1; i >= 0 && CalculateTotalPrice(i) < CalculateTotalPriceComputed(product, i); i--)
		{
			products[i + 1] = products[i];
		}

		products[i + 1] = product;
		count++;
		Finished = finish;
		
	}

	ProductComputed getItem()
	{
		unique_lock<mutex> lock(sortedMonitorLock);

		while (count == 0)
		{
			cv.wait(lock);
		}

		count--;
		ProductComputed output = products[count];
		cv.notify_all();
		return output;
	}

	bool IsEmpty() 
	{
		return count == 0;
	}
};

//inicijuojamas rezultatu monitorius
ResultMonitor resultMonitor;

//nuskaito pradinius duomenis i vektoriu
vector<Product> ReadFile(const string& filePath) 
{
	ifstream in(filePath);
	nlohmann::json input = nlohmann::json::parse(in);
	vector<Product> parsedData;

	for (auto& data : input)
	{
		Product dataInput{ data["name"],data["quantity"], data["price"] };
		// ideda i vektoriaus gala
		parsedData.push_back(dataInput);
	}

	in.close();
	return parsedData;
}

//rezultatu isvedimas i .txt faila
void WriteResultsToFile(string path, vector<Product> products) 
{
	ofstream out(path);
	out << "Pradiniai duomenys" << endl;
	out << "----------------------------------------------------------------------" << endl;
	out << "|" << setw(17) << "Product Name" << setw(15) << "Quantity" << setw(15) << "Price" << setw(20) << "Total Price" << " |" << endl;
	out << "|--------------------------------------------------------------------|" << endl;
	for (int i = 0; i < products.size(); i++)
	{
		double calculation = products[i].quantity * products[i].price;
		out << "|" << i + 1 << setw(15) << products[i].name << setw(15) << products[i].quantity << setw(15) << products[i].price << setw(20) << calculation << "  |" << endl;
	}
	out << "-----------------------------------------------------------------------" << endl;
	out << endl;

	if (!resultMonitor.IsEmpty())
	{
		out << "Rezultatai" << endl;
		out << "----------------------------------------------------------------------" << endl;
		out << "|" << setw(17) << "Product Name" << setw(15) << "Quantity" << setw(15) << "Price" << setw(22) << "Total Price |" << endl;
		out << "|--------------------------------------------------------------------|" << endl;
		int index = 0;

		while (!resultMonitor.IsEmpty())
		{
			ProductComputed sortedProduct = resultMonitor.getItem();
			out << "|" << index + 1 << setw(15) << sortedProduct.product.name << setw(15) << sortedProduct.product.quantity << setw(15) << sortedProduct.product.price << setw(20) << sortedProduct.totalPrice << " |" << endl;
			index++;
		}
		out << "----------------------------------------------------------------------" << endl;
	}

	else
	{
		out << "Rezultatu nera.";
	}

	out.close();
}

//atlieka totalPrice skaiciavimo funkcija gijose
void WorkerThreadFunction() 
{
	while (!dataMonitor.get_finished()) 
	{
		Product product = dataMonitor.getItem();
		ProductComputed productComp;
		productComp.product = product;
		//productComp.totalPrice = product.quantity * product.price;
		productComp.changeTotalPrice();
		//this_thread::sleep_for(chrono::milliseconds(1500));

		if (productComp.totalPrice >= 1 && productComp.totalPrice <= filter) 
		{
			resultMonitor.addItemSorted(productComp, false);
		}
	}
}

int main() 
{
	vector<Product> products = ReadFile(dataFile2);
	thread threads[threadCount];

	for (int i = 0; i < threadCount; i++)
	{
		// inicijuojamos gijos
		threads[i] = thread(WorkerThreadFunction);
	}

	bool finished = false;
	for (int i = 0; i < products.size(); i++)
	{	
		dataMonitor.addItem(products[i], finished);


		//this_thread::sleep_for(chrono::milliseconds(1));
		//std::cout << i << "Dydis \n" << products.size() << "\n" << std::endl;    <--- posible deadlock solution. reiketu dar ir unique_lock sorted


		//taip kaip skaiciuoja vektoriai reikia -2 kad paimt paskutini elementa, kitaip bus out of bounds
		//jeigu paskutinis elementas, baigia darba - finished
		// nusetina flaga tame konteineryje. itakos turi addItem metodui
		if (i == products.size() - 2)
		{
			finished = true;
		}
	}

	for (int i = 0; i < threadCount; i++)
	{
		// laukia kol pasibaigs darbas visu giju
		threads[i].join();
	}

	WriteResultsToFile(outputFile, products);

	return 0;
}
