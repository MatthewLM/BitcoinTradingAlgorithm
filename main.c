/*
 
 Copyright 2014 Matthew Mitchell
 
 This file is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version. The full text of version 3 of the GNU
 General Public License can be found in the file LICENSE-GPLV3.
 
 This file is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this file.  If not, see <http://www.gnu.org/licenses/>.
 
 Additional Permissions under GNU GPL version 3 section 7
 
 When you convey a covered work in non-source form, you are not
 required to provide source code corresponding to the covered work.
 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <OpenCL/opencl.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <unistd.h>
#define RSI_COMBINATIONS 0
#define PPO_COMBINATIONS 54648
#define PM_COMBINATIONS 2300
#define MAX_COMBINATIONS PPO_COMBINATIONS

CURL * curl;

typedef struct{
	long int date;
	double value;
} DateDataRaw;

typedef struct{
	DateDataRaw * data;
	int length;
} DateData;

typedef struct{
	char * response;
	int length;
} ResponseStruct;

typedef struct{
	int signal;
	float profit;
} CombinationResult;

//float best_rsi_profit;
float best_ppo_profit;
float best_pm_profit;
float best_profit;
int best_period;
int best_signal;
int best_id;
int best_indicator;

size_t write_to_string(void * ptr,size_t size,size_t nmeb,ResponseStruct * res_struct){
	size_t bytes = size*nmeb;
	res_struct->length += bytes;
	res_struct->response = realloc(res_struct->response, res_struct->length);
	strncat(res_struct->response,ptr,bytes);
	return bytes;
}

void sec_sleep(int seconds){
	struct timespec wait_time;
	wait_time.tv_sec = seconds;
	wait_time.tv_nsec = 0;
	struct timespec unused;
	while (1){
		nanosleep(&wait_time, &unused);
		if (errno == EINTR){
			wait_time = unused;
		}else{
			break;
		}
	}
}

char * process_url(char * url,char * post){
	CURLcode res;
	curl_easy_reset(curl);
	if(curl){
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl, CURLOPT_POSTREDIR,CURL_REDIR_POST_ALL);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,20);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,1L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_8) AppleWebKit/535.1 (KHTML, like Gecko) Chrome/13.0.782.107 Safari/535.1");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,write_to_string);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		if (post) {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
		}
		ResponseStruct result;
		result.response = malloc(sizeof(char));
		result.response[0] = '\0';
		result.length = 1;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
		res = curl_easy_perform(curl);
		if (res) {
			printf("CURL FAILED WITH ERROR CODE %i FOR URL %s\n",(int)res,url);
			return NULL;
		}
		sec_sleep(1); //Prevent passing float limit.
		return result.response;
	}
	return NULL;
}

int needle_occurences(char * haystack,char * needle){
	for (int x = 0;; x++) {
		if (!(haystack = strstr(haystack, needle))) {
			return x;
		}else{
			haystack++;
		}
	}
}

void get_balances(float * usd,float * btc){
	return;
	char * response = process_url("https://api-test.tradehill.com/APIv1/USD/GetBalance","name=&pass=");
	char * ptr;
	char * ptr2;
	if (response && strcmp(response, (const char *)"{\"error\":\"Not logged in.\"}") && strlen(response)){
		ptr = strstr(response, "\"BTC_Available\": ");
		ptr += 18;
		ptr2 = strstr(ptr, "\"");
		*ptr2 = '\0';
		*btc = atof(ptr);
		ptr = ptr2 + 1;
		ptr = strstr(ptr, "\"USD_Available\": ");
		ptr += 18;
		ptr2 = strstr(ptr, "\"");
		*ptr2 = '\0';
		*usd = atof(ptr);
		free(response);
		errno = 1;
		return;
	}
	errno = 0;
}

void get_bid_and_ask(float * bid,float * ask){
	char * response = process_url("http://mtgox.com/api/0/data/ticker.php",NULL);
	char * ptr;
	char * ptr2;
	if (response && strlen(response)){
		ptr = strstr(response, "\"buy\":");
		ptr += 6;
		ptr2 = strstr(ptr, ",");
		*ptr2 = '\0';
		*ask = atof(ptr);
		ptr = ptr2 + 1;
		ptr = strstr(ptr, "\"sell\":");
		ptr += 7;
		ptr2 = strstr(ptr, "}");
		*ptr2 = '\0';
		*bid = atof(ptr);
		free(response);
		errno = 1;
		return;
	}
	errno = 0;
}

void cancel_outstanding_order(){
	return;
	char * response = process_url("https://api-test.tradehill.com/APIv1/USD/GetOrders","name=&pass=");
	char * ptr;
	char * ptr2;
	if (response && strlen(response) && strcmp(response, (const char *)"{\"error\":\"Not logged in.\"}")){
		if (strcmp(response, (const char *)"{\"orders\": []}")) {
			ptr = strstr(response, "\"oid\": ");
			if (ptr) {
				ptr += 7;
				ptr2 = strstr(ptr, ",");
				*ptr2 = '\0';
				//Cancel this order
				char * post_data = malloc(sizeof(char) * 77 + strlen(ptr));
				strcpy(post_data,"name=&pass=&oid=");
				strcat(post_data,ptr);
				process_url("https://api-test.tradehill.com/APIv1/USD/CancelOrder",post_data);
				free(response);
				errno = 1;
				return;
			}
		}
	}
	errno = 0;
}

void order(int order_type,float price,float bitcoins){
	return;
	if (price > 999999999){
		price = 999999999;
	}
	bitcoins *= 0.994;
	if (bitcoins > 999999999){
		bitcoins = 999999999;
	}
	printf("%.2f\n",bitcoins);
	char post_data[116];
	sprintf(post_data,"name=&pass=&price=%.2f&amount=%.2f",price,bitcoins);
	if (order_type) {
		process_url("https://api-test.tradehill.com/APIv1/USD/SellBTC", post_data);
	}else{
		process_url("https://api-test.tradehill.com/APIv1/USD/BuyBTC", post_data);
	}
}

DateData * get_prices(DateData * prior_prices){
	time_t earliest = time(NULL) - 1382400; //16 days
	int since;
	if (prior_prices->length) {
		since = prior_prices->data[prior_prices->length - 1].date;
	}else{
		since = earliest;
	}
	char * response = process_url("https://mtgox.com/code/data/getTrades.php",NULL);
	char * orig = response;
	if (response) {
		DateData * new_prices = malloc(sizeof(DateData));
		new_prices->length = needle_occurences(response, "\"date\":") + prior_prices->length + 1;
		new_prices->data = malloc(sizeof(DateDataRaw) * new_prices->length);
		int prices_ptr = 1;
		int latest_old_date = earliest;
		for (int x = 0; x < prior_prices->length; x++) {
			if (prior_prices->data[x].date > earliest) {
				new_prices->data[prices_ptr] = prior_prices->data[x];
				prices_ptr++;
				latest_old_date = prior_prices->data[x].date;
			}
		}
		free(prior_prices->data);
		free(prior_prices);
		char * ptr;
		char * ptr2;
		int date;
		while ((ptr = strstr(response, "\"date\":"))) {
			ptr += 7;
			ptr2 = strstr(ptr, ",");
			*ptr2 = '\0';
			date = atol(ptr);
			if (date > latest_old_date) {
				new_prices->data[prices_ptr].date = date;
				ptr = strstr(ptr2 + 1, "\"price\":\"");
				ptr += 9;
				ptr2 = strstr(ptr, "\"");
				*ptr2 = '\0';
				new_prices->data[prices_ptr].value = atof(ptr);
				prices_ptr++;
			}
			response = ptr2 + 1;
		}
		new_prices->length = prices_ptr;
		new_prices->data = realloc(new_prices->data, sizeof(DateDataRaw) * new_prices->length);
		new_prices->data[0].date = earliest;
		new_prices->data[0].value = new_prices->data[1].value;
		free(orig);
		errno = 1;
		return new_prices;
	}
	errno = 0;
	return prior_prices;
}

void print_context_error(const char * error,const void * foo,size_t bar,void * baz){
	printf("ENCOUNTERED OPENCL CONTEXT ERROR: %s\n",error);
}

void open_high_low_close(DateData * prices,int seconds,float (* ohlc)[5],int ohlc_len){
	int end_of_period = seconds*(prices->data[0].date/seconds) + seconds;
	int price = 0;
	int y;
	for (int x = 0; x < ohlc_len; x++) {
		ohlc[x][0] = prices->data[price].value;
		ohlc[x][1] = 0;
		ohlc[x][2] = INT_MAX;
		for (y = price; y < prices->length; y++) {
			if (prices->data[y].date > end_of_period) {
				break;
			}
			if (prices->data[y].value > ohlc[x][1]) {
				ohlc[x][1] = prices->data[y].value;
			}
			if (prices->data[y].value < ohlc[x][2]) {
				ohlc[x][2] = prices->data[y].value;
			}
		}
		price = y - 1;
		ohlc[x][3] = prices->data[price].value;
		ohlc[x][4] = (ohlc[x][1] + ohlc[x][2] + ohlc[x][3]*2)/4;
		end_of_period += seconds;
	}
}

void exponential_moving_average(float (* ohlc)[5],int a,float * moving_averages,int ohlc_len){
	int amount = (a+1)*2;
	float weight = 2.0/(amount+1);
	double total = 0;
	for (int x = 0; x < amount; x++){
		total += ohlc[x][4];
	}
	moving_averages[amount-1] = total/amount;
	double av = moving_averages[amount-1];
	for (int x = amount; x < ohlc_len; x++){
		av = ohlc[x][4] * weight + av * (1 - weight);
		moving_averages[x] = av;
	}
}

bool execute_algorithm(DateData * prices,float possible_price,cl_command_queue commands,cl_device_id device_id,cl_kernel PPO_kernel,cl_kernel PM_kernel,cl_mem ohlc_data,cl_mem moving_averages_data,cl_mem output){
	float ohlc[2308][5];
	int ohlc_len;
	float moving_averages[12][2306];
	//CombinationResult RSI_results[RSI_COMBINATIONS];
	CombinationResult PPO_results[PPO_COMBINATIONS];
	CombinationResult PM_results[PM_COMBINATIONS];
	best_profit = 0.0;
	//best_rsi_profit = 0.0;
	best_ppo_profit = 0.0;
	best_pm_profit = 0.0;
	size_t local;
	int periods_count = 0;
	size_t global;
	for (int p = -7; p < 9; p++){ //Loop through periods on host.
		sec_sleep(1);
		int period = 2000 + p * 200;
		periods_count++;
		//Calculate OHLC data
		ohlc_len = 1382400.0/period + 1;
		open_high_low_close(prices, period,ohlc,ohlc_len);
		ohlc[ohlc_len-1][4] = (ohlc[ohlc_len - 1][1] + ohlc[ohlc_len - 1][2] + possible_price*2)/4;
		//Calculate moving averages on host
		for (int a = 0; a < 12; a++) {
			exponential_moving_average(ohlc,a,moving_averages[a],ohlc_len);
		}
		//OpenCL part
		int err = clEnqueueWriteBuffer(commands, ohlc_data, CL_TRUE, 0, sizeof(float) * ohlc_len * 5, ohlc, 0, NULL, NULL);
		if (err != CL_SUCCESS){
			printf("ERROR: Failed to write to OHLC array.\n");
			return false;
		}
		/*//RSI combinations
		err  = clSetKernelArg(RSI_kernel, 0, sizeof(cl_mem), &ohlc_data);
		err |= clSetKernelArg(RSI_kernel, 1, sizeof(cl_mem), &output);
		err |= clSetKernelArg(RSI_kernel, 2, sizeof(int), &ohlc_len);
		if (err != CL_SUCCESS){
			printf("ERROR: Failed to set arguments. %d\n", err);
			return false;
		}
		err = clGetKernelWorkGroupInfo(RSI_kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
		if (err != CL_SUCCESS){
			printf("Error: Failed to retrieve kernel work group info! %d\n", err);
			return false;
		}
		size_t global = (RSI_COMBINATIONS / local + 1)*local;
		err = clEnqueueNDRangeKernel(commands, RSI_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
		if (err){
			printf("Error: Failed to execute kernel!\n");
			return false;
		}
		clFinish(commands);
		//Get results
		err = clEnqueueReadBuffer(commands, output, CL_TRUE, 0, sizeof(CombinationResult) * RSI_COMBINATIONS, RSI_results, 0, NULL, NULL);  
		if (err != CL_SUCCESS){
			printf("Error: Failed to read output array! %d\n", err);
			return false;
		}*/
		//PPO combinations
		err = clEnqueueWriteBuffer(commands, moving_averages_data, CL_TRUE, 0, sizeof(float) * 6900, moving_averages, 0, NULL, NULL);
		if (err != CL_SUCCESS){
			printf("ERROR: Failed to write to MA array.\n");
			return false;
		}
		err  = clSetKernelArg(PPO_kernel, 0, sizeof(cl_mem), &ohlc_data);
		err |= clSetKernelArg(PPO_kernel, 1, sizeof(cl_mem), &moving_averages_data);
		err |= clSetKernelArg(PPO_kernel, 2, sizeof(cl_mem), &output);
		err |= clSetKernelArg(PPO_kernel, 3, sizeof(int), &ohlc_len);
		if (err != CL_SUCCESS){
			printf("ERROR: Failed to set arguments. %d\n", err);
			return false;
		}
		err = clGetKernelWorkGroupInfo(PPO_kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
		if (err != CL_SUCCESS){
			printf("Error: Failed to retrieve kernel work group info! %d\n", err);
			return false;
		}
		global = (PPO_COMBINATIONS / local + 1)*local;
		err = clEnqueueNDRangeKernel(commands, PPO_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
		if (err){
			printf("Error: Failed to execute kernel!\n");
			return false;
		}
		clFinish(commands);
		//Get results
		err = clEnqueueReadBuffer(commands, output, CL_TRUE, 0, sizeof(CombinationResult) * PPO_COMBINATIONS, PPO_results, 0, NULL, NULL);
		if (err != CL_SUCCESS){
			printf("Error: Failed to read output array! %d\n", err);
			return false;
		}
		//PM combinations
		err  = clSetKernelArg(PM_kernel, 0, sizeof(cl_mem), &ohlc_data);
		err |= clSetKernelArg(PM_kernel, 1, sizeof(cl_mem), &output);
		err |= clSetKernelArg(PM_kernel, 2, sizeof(int), &ohlc_len);
		if (err != CL_SUCCESS){
			printf("ERROR: Failed to set PM arguments. %d\n", err);
			return false;
		}
		err = clGetKernelWorkGroupInfo(PM_kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
		if (err != CL_SUCCESS){
			printf("Error: Failed to retrieve kernel work group info! %d\n", err);
			return false;
		}
		global = (PM_COMBINATIONS / local + 1)*local;
		err = clEnqueueNDRangeKernel(commands, PM_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
		if (err){
			printf("Error: Failed to execute PM kernel!\n");
			return false;
		}
		clFinish(commands);
		//Get results
		err = clEnqueueReadBuffer(commands, output, CL_TRUE, 0, sizeof(CombinationResult) * PM_COMBINATIONS, PM_results, 0, NULL, NULL);
		if (err != CL_SUCCESS){
			printf("Error: Failed to read output array! %d\n", err);
			return false;
		}
		//Find best RSI,PPO and PM
		/*for (int x = 0; x < RSI_COMBINATIONS; x++) {
			if (RSI_results[x].profit > best_rsi_profit) {
				best_rsi_profit = RSI_results[x].profit;
			}
			if (RSI_results[x].profit > best_profit) {
				best_profit = RSI_results[x].profit;
				best_period = period;
				best_signal = RSI_results[x].signal;
				best_id = x;
				best_indicator = 0;
			}
		}*/
		for (int x = 0; x < PPO_COMBINATIONS; x++) {
			if (PPO_results[x].profit > best_ppo_profit) {
				best_ppo_profit = PPO_results[x].profit;
			}
			if (PPO_results[x].profit > best_profit) {
				best_profit = PPO_results[x].profit;
				best_period = period;
				best_signal = PPO_results[x].signal;
				best_id = x;
				best_indicator = 1;
			}
		}
		for (int x = 0; x < PM_COMBINATIONS; x++) {
			if (PM_results[x].profit > best_pm_profit) {
				best_pm_profit = PM_results[x].profit;
			}
			if (PM_results[x].profit > best_profit) {
				best_profit = PM_results[x].profit;
				best_period = period;
				best_signal = PM_results[x].signal;
				best_id = x;
				best_indicator = 2;
			}
		}
	}
	return true;
}

DateData * load_prices(){
	FILE * file = fopen("price.dat", "rb");
	DateData * prices = malloc(sizeof(DateData));
	if (!file) {
		printf("Cannot open prices file.\n");
		prices->length = 0;
		prices->data = NULL;
		fclose(file);
		return prices;
	}
	fseek(file, 0, SEEK_END);
	int len = ftell(file) + 1;
	prices->length = len/sizeof(DateDataRaw);
	prices->data = malloc(sizeof(DateDataRaw) * prices->length);
	fseek(file, 0, SEEK_SET);
	fread(prices->data, len, 1, file);
	fclose(file);
	return prices;
}

void save_prices(DateData * prices){
	FILE * file = fopen("price.dat", "wb");
	if (!file) {
		printf("Cannot open prices file.\n");
		fclose(file);
		return;
	}
	fwrite(prices->data, sizeof(DateDataRaw), (size_t)prices->length, file);
	fclose(file);
}

void load_sd(float * profit,float * buy_at){
	FILE * file = fopen("session.dat", "rb");
	if (!file) {
		printf("Cannot open session file.\n");
		fclose(file);
		return;
	}
	fseek(file, 0, SEEK_END);
	int len = ftell(file) + 1;
	if (len < 2){
		fclose(file);
		return;
	}
	fseek(file, 0, SEEK_SET);
	fread(profit, sizeof(float), 1, file);
	fread(buy_at, sizeof(float), 1, file);
	fclose(file);
}

void save_sd(float * profit,float * buy_at){
	FILE * file = fopen("session.dat", "wb");
	if (!file) {
		printf("Cannot open session file.\n");
		fclose(file);
		return;
	}
	fwrite(profit, sizeof(float), 1, file);
	fwrite(buy_at, sizeof(float), 1, file);
	fclose(file);
}

int main (int argc, const char * argv[]) {
	float usd,btc,bid,ask,order_price,possible_price;
	DateData * prices = load_prices();
	float profit = 100.0;
	float buy_at = 0.0;
	load_sd(&profit, &buy_at);
	best_period = 2400;
	//Init CURL
	curl = curl_easy_init();
	printf("LOADED LIBCURL: %s\n",curl_version());
	//Init OpenCL
	int error;
	cl_device_id device_id;
	cl_context context;
	cl_command_queue commands;
	cl_program program;
	//cl_kernel RSI_kernel;
	cl_kernel PPO_kernel;
	cl_kernel PM_kernel;
	cl_mem output;
	cl_mem ohlc_data;
	cl_mem moving_averages_data;
	error = clGetDeviceIDs(NULL,CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
	if (error != CL_SUCCESS) {
		printf("ERROR: Could not discover GPU.");
		return EXIT_FAILURE;
	}
	context = clCreateContext(0, 1, &device_id, &print_context_error, NULL, NULL);
	if (!context){
		printf("ERROR: Device not ready to create context.\n");
		return EXIT_FAILURE;
	}
	commands = clCreateCommandQueue(context, device_id, 0, NULL);
	if (!commands){
		printf("ERROR: Command queue would not initialise.\n");
		return EXIT_FAILURE;
	}
	//Obtain source code
	FILE * file = fopen("kernels.cl", "rb");
	if (!file) {
		printf("ERROR: Cannot open kernels.cl file.\n");
		return EXIT_FAILURE;
	}
	fseek(file, 0, SEEK_END);
	int len = ftell(file) + 1;
	char * source_code = malloc(len);
	fseek(file, 0, SEEK_SET);
	fread(source_code, len, 1, file);
	fclose(file);
	source_code[len - 1] = '\0';
	program = clCreateProgramWithSource(context, 1, (const char **) &source_code, NULL, NULL);
	if (!program){
		printf("ERROR: Couldn't create program from source.\n");
		return EXIT_FAILURE;
	}
	error = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (error != CL_SUCCESS){
		printf("ERROR: Failed to build program executable.\n");
		return EXIT_FAILURE;
	}
	free(source_code);
	/*RSI_kernel = clCreateKernel(program, "RSI", &error);
	if (!RSI_kernel || error != CL_SUCCESS){
		printf("ERROR Failed to create compute kernel for RSI.\n");
		return EXIT_FAILURE;
	}*/
	PPO_kernel = clCreateKernel(program, "PPO", &error);
	if (!PPO_kernel || error != CL_SUCCESS){
		printf("ERROR Failed to create compute kernel for PPO.\n");
		return EXIT_FAILURE;
	}
	PM_kernel = clCreateKernel(program, "PM", &error);
	if (!PM_kernel || error != CL_SUCCESS){
		printf("ERROR Failed to create compute kernel for PM.\n");
		return EXIT_FAILURE;
	}
	output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(CombinationResult) * MAX_COMBINATIONS, NULL, NULL);
	if (!output){
		printf("ERROR: Failed to allocate memory for results\n");
		return EXIT_FAILURE;
	} 
	ohlc_data = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * 2308 * 5, NULL, NULL);
	if (!ohlc_data){
		printf("ERROR: Failed to allocate memory for ohlc data\n");
		return EXIT_FAILURE;
	}
	moving_averages_data = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float) * 13824, NULL, NULL);
	if (!moving_averages_data){
		printf("ERROR: Failed to allocate memory for moving averages data\n");
		return EXIT_FAILURE;
	}
	printf("LOADED - %i\nBEGIN IN 5 SECONDS\n",getpid());
	sec_sleep(5);
	while (1) {
		get_bid_and_ask(&bid,&ask);
		cancel_outstanding_order();
		get_balances(&usd, &btc);
		prices = get_prices(prices);
		save_prices(prices);
		if (errno) {
			int end = prices->data[prices->length - 1].date;
			int start_time = time(NULL);
			//Parallel Computing Algorithm
			possible_price = prices->data[prices->length - 1].value;
			if (ask < possible_price) {
				possible_price = ask;
			}else if (bid > possible_price) {
				possible_price = bid;
			}
			bool results = execute_algorithm(prices,possible_price,commands,device_id,PPO_kernel,PM_kernel,ohlc_data,moving_averages_data,output);
			if (results) {
				if (end-prices->data[1].date > 172800) { //Two days of data required
					order_price = prices->data[prices->length - 1].value;
					if(buy_at != 0.0){ //Sell bought
						if (best_signal == 2){
							if (ask < order_price) {
								order_price = ask-0.01;
							}
							order(1,order_price,btc);
							profit *= order_price/buy_at;
							profit *= 0.9946;
							buy_at = 0.0;
						}
					}else if(best_signal == 0){ //Buy
						if (bid > order_price) {
							order_price = bid+0.01;
						}
						order(1,order_price,(float)((usd*100)/order_price)/100);
						buy_at = order_price;
						profit *= 0.9946;
					}
				}
				save_sd(&profit, &buy_at);
				printf("///////////////////////////////////////////////\n\n");
				printf("COMPUTED %i ALGORITHM COMBINATIONS IN %i SECONDS\n",(PPO_COMBINATIONS+PM_COMBINATIONS)*6,time(NULL) - start_time);
				printf("THE ALGORITHM HAS DETERMINED THAT:\nSIGNAL = ");
				printf((!best_signal)? "BUY" : ((best_signal == 1) ? "HOLD" : "SELL"));
				printf("\nSIMULATED PPO PROFIT = %.2f%%\nSIMULATED PM PROFIT = %.2f%%\n",best_ppo_profit-100,best_pm_profit-100);
				printf("PERIOD = %i\n",best_period);
				printf("COMBINATION INDEX = %i\n",best_id);
				printf("PROFIT = %.2f%%\n\n",profit-100);
				printf("USD BALANCE = %.2f\n",usd);
				printf("BTC BALANCE = %.2f\n",btc);
				printf("\nPRICE LENGTH = %i",prices->length);
				printf("\nPRICE RANGE DATE = %li",end-prices->data[1].date);
				printf("\n\n///////////////////////////////////////////////\n");
			}else{
				printf("ERROR WITH COMPUTATION\n");
			}
		}else{
			printf("ERROR: There has been an error with retrieving the price data. Check connection.\n\a");
		}
		sec_sleep(0);
	}
	curl_easy_cleanup(curl);
	return EXIT_SUCCESS;
}