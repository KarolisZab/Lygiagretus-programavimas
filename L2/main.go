package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"reflect"
	"sort"
	"strings"
	"sync"
)

type Product struct {
	Name     string
	Quantity int
	Price    float32
}

func (product *Product) toString() string {
	//fmt.Printf("%s %d %f\n", c.Brand, c.MakeYear, c.Mileage)
	return fmt.Sprintf("|%20v|%20v|%20v|%20v|", product.Name, product.Quantity, product.Price, product.totalPrice())
}

func (product *Product) totalPrice() float32 {
	totalPrice := float32(product.Quantity) + product.Price

	return totalPrice
}

const filePath string = "data/IFF_9_6_ŽabinskisK_L1_dat.json"

///const filePath string = "data/IFF_9_6_ŽabinskisK_L1_dat_2.json"
///const filePath string = "data/IFF_9_6_ŽabinskisK_L1_dat_3.json"

const resultsFilePath string = "data/IFF_9_ŽabinskisK_L1_rez4.txt"

///const resultsFilePath string = "data/IFF_9_ŽabinskisK_L1_rez2.txt"
///const resultsFilePath string = "data/IFF_9_ŽabinskisK_L1_rez3.txt"

const filterCondition int = 100
const workersCount int = 4

func main() {
	// Reads json data and parses it to Product array type
	data := ReadData(filePath)
	products := ParseJsonData(data)
	n := len(products)

	// Counters for synchronization
	group := sync.WaitGroup{}
	workers := sync.WaitGroup{}

	// Channels initialization
	main := make(chan Product)
	dataChan := make(chan Product)
	filtered := make(chan Product)
	results := make(chan Product)

	// Starts data array and results array management goroutines
	group.Add(2)
	go DataWorkerRoutine(n, main, dataChan, &group)
	go ResultsRoutine(filtered, results, &group)

	// Starts workers
	workers.Add(workersCount)
	for i := 0; i < workersCount; i++ {
		go WorkerRoutine(dataChan, filtered, &workers)
	}

	// Sends data items one by one to the data management goroutine
	for _, product := range products {
		main <- product
	}

	// Closes channels for stopping goroutines
	close(main)
	workers.Wait()
	close(filtered)

	// Retrieves filtered products from result goroutine
	var filteredProducts []Product
	for product := range results {
		filteredProducts = append(filteredProducts, product)
	}

	group.Wait()

	WriteResultsToFile(products, filteredProducts)
}

//ResultsRoutine Manages results array and send data to the main goroutine
func ResultsRoutine(filtered chan Product, resultsChan chan Product, group *sync.WaitGroup) {
	defer close(resultsChan)
	defer group.Done()

	var results []Product
	closedChannel := false

	for {
		var cases []reflect.SelectCase

		if !closedChannel {
			cases = append(cases, reflect.SelectCase{
				Dir:  reflect.SelectRecv,
				Chan: reflect.ValueOf(filtered),
			})
		} else {
			cases = append(cases, reflect.SelectCase{
				Dir:  reflect.SelectRecv,
				Chan: reflect.ValueOf(nil),
			})
		}

		if len(results) > 0 {
			cases = append(cases, reflect.SelectCase{
				Dir: reflect.SelectDefault,
			})
		}

		// Loop exit condition
		if !cases[0].Chan.IsValid() && len(cases) == 1 {
			return
		}

		chosen, item, ok := reflect.Select(cases)
		switch chosen {
		case 0:
			if !ok {
				closedChannel = true
			} else {
				results = append(results, item.Interface().(Product))
			}
		default:
			if closedChannel {
				resultsChan <- results[0]
				results = results[1:]
			}
		}
	}
}

//WorkerRoutine Filters given data and sends it to the results goroutine
func WorkerRoutine(dataChan chan Product, filtered chan Product, group *sync.WaitGroup) {
	defer group.Done()

	var products []Product
	for product := range dataChan {
		products = append(products, product)
		if int(product.totalPrice()) <= filterCondition {
			filtered <- product
		}
	}
}

//DataWorkerRoutine Manages given data from the main and sends it to the workers
func DataWorkerRoutine(n int, main chan Product, dataChan chan Product, group *sync.WaitGroup) {
	defer close(dataChan)
	defer group.Done()
	var data []Product

	channelClosed := false
	for {
		// Activates and deactivates channels
		var cases []reflect.SelectCase

		// If data array is full deactivates channel from the main
		if len(data) <= int(n/2) && !channelClosed {
			cases = append(cases, reflect.SelectCase{
				Dir:  reflect.SelectRecv,
				Chan: reflect.ValueOf(main),
			})
		} else {
			cases = append(cases, reflect.SelectCase{
				Dir:  reflect.SelectRecv,
				Chan: reflect.ValueOf(nil),
			})
		}

		// If there is data in the data array, opens channel to workers
		if len(data) > 0 {
			cases = append(cases, reflect.SelectCase{
				Dir: reflect.SelectDefault,
			})
		}

		// Loop exit condition
		if !cases[0].Chan.IsValid() && len(cases) == 1 {
			return
		}

		// Dynamic select for managing channels
		chosen, item, ok := reflect.Select(cases)
		switch chosen {
		case 0:
			if !ok {
				channelClosed = true
			} else {
				data = append(data, item.Interface().(Product))
			}
		default:
			dataChan <- data[0]
			data = data[1:]
		}
	}
}

// Parse json data to Car array
func ParseJsonData(jsonData []byte) []Product {
	var products []Product
	err := json.Unmarshal([]byte(jsonData), &products)
	if err != nil {
		log.Fatal(err)
	}

	return products
}

// Reads data in bytes from given file
func ReadData(filePath string) []byte {
	data, err := ioutil.ReadFile(filePath)
	if err != nil {
		log.Fatal(err)
	}

	return data
}

//WriteResultsToFile Writes original data and results data to the given results file in table format
func WriteResultsToFile(originalData []Product, results []Product) {
	file, err := os.OpenFile(resultsFilePath, os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		fmt.Printf("Failed writing to file %s", err)
	}

	dataWriter := bufio.NewWriter(file)

	// Table header row
	_, _ = dataWriter.WriteString(fmt.Sprintf("%55v\n", "Pradiniai duomenys"))
	_, _ = dataWriter.WriteString(strings.Repeat("-", 84) + "\n")
	_, _ = dataWriter.WriteString(fmt.Sprintf("|%20v|%20v|%20v|%20v|\n", "Produktas", "Kiekis",
		"Kaina", "Visa kaina"))
	_, _ = dataWriter.WriteString(strings.Repeat("-", 84) + "\n")

	for _, product := range originalData {
		_, _ = dataWriter.WriteString(product.toString() + "\n")
	}

	sort.Slice(results, func(i, j int) bool {
		return results[i].totalPrice() < results[j].totalPrice()
	})

	// Table header row
	_, _ = dataWriter.WriteString(fmt.Sprintf("\n\n%48v\n", "Rezultatas"))
	_, _ = dataWriter.WriteString(strings.Repeat("-", 84) + "\n")
	_, _ = dataWriter.WriteString(fmt.Sprintf("|%20v|%20v|%20v|%20v|\n", "Produktas", "Kiekis",
		"Kaina", "Visa kaina"))
	_, _ = dataWriter.WriteString(strings.Repeat("-", 84) + "\n")

	for _, product := range results {
		_, _ = dataWriter.WriteString(product.toString() + "\n")
	}

	_, _ = dataWriter.WriteString(fmt.Sprintf("Is viso prekiu: %v", len(results)))

	_ = dataWriter.Flush()
	_ = file.Close()
}
