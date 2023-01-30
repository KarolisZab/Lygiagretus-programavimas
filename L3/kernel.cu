#define __CUDACC__
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include <cuda.h>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

class Item
{
public:
    char itemTitle[256];
    int itemCount;
    double itemCost;
    char res[256];
    int fp_sum;
};

const string dataFile = "IFF_0_6_ŽabinskisK_L1_dat_1.json"; // visi tinka
//const string dataFile = "IFF_0_6_ŽabinskisK_L1_dat_2.json"; // kaikurie tinka
//const string dataFile = "IFF_0_6_ŽabinskisK_L1_dat_3.json"; // nei vienas netinka
const string resultFile = "IFF_0_6_ŽabinskisK_L3_rez.txt";

void readItemsFiles(vector<Item>* items) {
    ifstream stream(dataFile);
    json allItemsJson = json::parse(stream);

    auto allItems = allItemsJson["items"];
    for (auto& new_items : allItems) {
        Item tempItem;
        string n = new_items["title"];
        //Returns a pointer to an array that contains a null-terminated sequence of characters (i.e., a C-string) 
        // representing the current value of the string object.
        strcpy(tempItem.itemTitle, n.c_str());
        tempItem.itemCount = new_items["count"];
        tempItem.itemCost = new_items["cost"];
        items->push_back(tempItem);
    }
    stream.close();
}

void writeListToFile(vector<Item>& items, string fileName) {
    ofstream file;
    file.open(fileName, ios::out);
    file << setw(33) << "Pradiniai duomenys" << endl
        << "--------------------------------------------------------------" << endl
        << setw(5) << "Nr. |" << setw(25) << "Pavadinimas |" << setw(15) << "Kiekis |" << setw(17) << "Kaina |" << endl
        << "--------------------------------------------------------------" << endl;

    for (int i = 0; i < items.size(); i++)
    {
        file << setw(5) << to_string(i) << setw(23) << items[i].itemTitle << " |" << setw(13) << to_string(items[i].itemCount) << " |" << setw(15) << to_string(items[i].itemCost) << " |" << endl;
    }
    file << "--------------------------------------------------------------" << endl << endl;
    file.close();
}

void writeResultToFile(Item items[], string fileName, int res_size) {
    ofstream file;
    file.open(fileName, ios::app);
    file << setw(39) << "Rezultatai" << endl
        << "---------------------------------------------------------------------------------" << endl
        << setw(5) << "Nr. |" << setw(25) << "Pavadinimas |" << setw(15) << "Kiekis |" << setw(17) << "Kaina |" << setw(16) << "Teksto rezultatas |" << endl
        << "---------------------------------------------------------------------------------" << endl;

    for (int i = 0; i < res_size; i++)
    {
        file << setw(5) << to_string(i) << setw(23) << items[i].itemTitle << " |" << setw(13) << to_string(items[i].itemCount) << " |"
            << setw(15) << to_string(items[i].itemCost) << " |" << setw(17) << (items[i].res) << " |" << setw(20) << to_string(items[i].fp_sum) << "|" << endl;
    }
    file << "---------------------------------------------------------------------------------" << endl;
    file << "Total Price:"  << endl;
    file.close();
}

//__device__  Funkcijos, vykdomos GPU ir kviečiamos iš GPU

// Appends char array to other char array
__device__ void gpu_strcpy(char* dest, const char* src) {
    int i = 0;
    do {
        dest[i] = src[i];
    } while (src[i++] != 0);
}

__device__ void gpu_string(char* dest, const char* src) {
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = '<';
    dest[3] = '2';
    dest[4] = '0';
    dest[5] = '0';
}

//__global__ Funkcijos, vykdomos GPU, bet kviečiamos iš CPU
__global__ void gpu_func(Item* device_items, Item* device_results, int* device_array_size, int* device_slice_size, int* device_result_count) {
    
    // compute start index
    unsigned long start_index = *device_slice_size * threadIdx.x;
    unsigned long end_index;


    // compute end index. Last thread takes all remaining elements in case they are not split evenly between threads
    if (threadIdx.x == blockDim.x - 1)
        end_index = *device_array_size;
    else
        end_index = *device_slice_size * (threadIdx.x + 1);

    auto fp_sum = 0;

    for (int i = start_index; i < end_index; i++) {
        double quantity = device_items[i].itemCount;
        double price = device_items[i].itemCost;
        double fullPrice = quantity * price;

        if (fullPrice <= 200) {
            Item Item;
            gpu_strcpy(Item.itemTitle, device_items[i].itemTitle);
            Item.itemCount = device_items[i].itemCount;
            Item.itemCost = device_items[i].itemCost;
            gpu_string(Item.res, device_items[i].itemTitle);
            Item.fp_sum += device_items[i].itemCost * device_items[i].itemCount;
            // sudeda dvi reiksmes
            int index = atomicAdd(device_result_count, 1);
            device_results[index] = Item;
        }
    }
}

const int SIZE = 256;
// Intel(R) Core(TM) i5-8300H CPU - turi 4 bandruolius
// Palaiko 8 gijas
const int THREADS = 8;

int main()
{
    vector<Item> data;
    readItemsFiles(&data);

    Item* items = &data[0];
    Item results[SIZE];
    // one thread processes one slice of data. Slice size is equal to total count divided by number of threads
    int slice_size = SIZE / THREADS;
    int result_count = 0;

    Item* device_items;
    Item* device_results;
    int* device_array_size;
    int* device_slice_size;
    int* device_result_count;


    //GPU atmintyje išskiria nurodytą kiekį atminties. 
    cudaMalloc((void**)&device_items, SIZE * sizeof(Item));
    cudaMalloc((void**)&device_array_size, sizeof(int));
    cudaMalloc((void**)&device_slice_size, sizeof(int));
    cudaMalloc((void**)&device_result_count, sizeof(int));
    cudaMalloc((void**)&device_results, SIZE * sizeof(Item));

    //Funkcijos, vykdomos GPU ir kviečiamos iš GPU
    //cudaMemcpyHostToHost iš CPU į CPU
    //cudaMemcpyHostToDevice iš CPU į GPU
    //cudaMemcpyDeviceToHost iš GPU į CPU
    //cudaMemcpyDeviceToDevice iš GPU į GPU

    //Iš CPU siunčiami(kopijuojami) duomenys į GPU
    cudaMemcpy(device_items, items, SIZE * sizeof(Item), cudaMemcpyHostToDevice);
    cudaMemcpy(device_array_size, &SIZE, sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(device_slice_size, &slice_size, sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(device_result_count, &result_count, sizeof(int), cudaMemcpyHostToDevice);

    gpu_func << <1, THREADS >> > (device_items, device_results, device_array_size, device_slice_size, device_result_count);
    //Blokuoja CPU kodą, kol GPU pabaigs visą jam priskirtą darbą.
    cudaDeviceSynchronize();

    cudaMemcpy(&results, device_results, SIZE * sizeof(Item), cudaMemcpyDeviceToHost);
    int RES_SIZE = 0;
    cudaMemcpy(&RES_SIZE, device_result_count, sizeof(int), cudaMemcpyDeviceToHost);

    writeListToFile(data, resultFile);
    writeResultToFile(results, resultFile, RES_SIZE);


    //Atlaisvina GPU išskirtą atmintį. 
    cudaFree(device_array_size);
    cudaFree(device_items);
    cudaFree(device_results);
    cudaFree(device_result_count);
    cudaFree(device_slice_size);

    return 0;
}

