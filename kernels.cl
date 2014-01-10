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

typedef struct{
	int signal;
	float profit;
} CombinationResult;

typedef struct{
	long int date;
	double value;
} DateDataRaw;
__kernel void PPO(__global float (* ohlc)[5],__global float ** averages,__global CombinationResult * results,int ohlc_len,int offset){
	int index = get_global_id(0);
	if (index >= 54648){
		return;
	}
	int ind = index;
	int av_ind = index % 66;
	int yav;
	int xav;
	int y = 0;
	for (int x = 11; x >= 0; x--){
		y += x;
		if (av_ind < y) {
			xav = 11-x;
			yav = xav + av_ind + x - y + 1;
			break;
		}
	}
	index /= 66;
	int EMA_amount = index % 23 + 2;
	index /= 23;
	int sell = -(index % 6)/5;
	index /= 6;
	int buy = (index % 6)/5;
	//Calculate PPO
	float PPO[2];
	float sma_total = 0.0;
	float weight = 2.0/(EMA_amount+1);
	float EMA[2];
	float divergence[2];
	divergence[1] = NAN;
	int last_signal = 1;
	float buy_at = 0.0;
	float profit = 100;
	for (int x = yav * 2 + 1;; x < ohlc_len; x++) {
		PPO[0] = PPO[1];
		//printf("XAV: %.2f YAV: %.2f\n",averages[xav][x],averages[yav][x]);
		PPO[1] = 100*(averages[xav][x+offset]-averages[yav][x+offset])/averages[yav][x+offset];
		//printf("PPO%i = %.2f\n",x,PPO[1]);
		if(x - yav + 2 > EMA_amount){
			//EMA
			if(sma_total){
				//printf("SMAT: %.2f EMAA: %i\n",sma_total,EMA_amount);
				EMA[1] = sma_total/EMA_amount;
				sma_total = 0;
			}
			EMA[0] = EMA[1];
			EMA[1] =  PPO[1] * weight + EMA[0] * (1 - weight);
			//printf("EMA%i = %.2f\n",x,EMA[1]);
			//Calculate signal
			divergence[0] = divergence[1];
			divergence[1] = PPO[1] - EMA[1];
			//printf("DIV%i = %.2f\n",x,divergence[1]);
			if (divergence[0] != NAN) {
				if (PPO[0] > 0 && PPO[1] < 0){
					last_signal = 2;
				}else if (PPO[0] < 0 && PPO[1] > 0){
					last_signal = 0;
				}else if (divergence[0] < buy && divergence[1] > buy) {
					last_signal = 0;
				}else if (divergence[0] > sell && divergence[1] < sell) {
					last_signal = 2;
				}else if (divergence[0] > buy && divergence[1] < buy){
					last_signal = 1;
				}else if (divergence[0] < sell && divergence[1] > sell){
					last_signal = 1;	
				}
				//Modify profits
				if(buy_at != 0.0){ //Sell bought
					if (last_signal == 2) {
						profit *= ohlc[x+offset][3]/buy_at;
						profit *= 0.9946;
						buy_at = 0.0;
					}
				}else if(last_signal == 0){ //Buy
					buy_at = ohlc[x][3];
					profit *= 0.9946;
				}
			}
		}else{
			//SMA
			sma_total += PPO[1];
		}
	}
	results[ind].signal = last_signal;
	results[ind].profit = profit;
}

__kernel void PM(__global float (* ohlc)[5],__global CombinationResult * results,int ohlc_len){
	int index = get_global_id(0);
	int ind = index;
	if (index >= 2300) {
		return;
	}
	int amount = index % 23 + 2;
	index /= 23;
	float sell = (index % 10 - 13) * 0.1;
	index /= 10;
	float buy = (index % 10 + 4) * 0.1;
	//Calculate period momentum
	float pm[2]; //Two because only two is needed
	pm[1] = NAN;
	int num;
	int last_signal = 1;
	float buy_at = 0.0;
	float profit = 100;
	for(int x = amount-1; x < ohlc_len; x++){
		pm[0] = pm[1];
		pm[1] = 0.0;
		num = 0;
		for (int y = x-amount+1; y <= x; y++) {
			pm[1] += 100*(ohlc[x][4]-ohlc[y][4])/ohlc[y][4];
			num++;
		}
		pm[1] /= num;
		if (pm[0] != NAN) {
			if (pm[0] < buy && pm[1] > buy) {
				last_signal = 0;
			}else if (pm[0] > sell && pm[1] < sell) {
				last_signal = 2;
			}else if (pm[0] > buy && pm[1] < buy) {
				last_signal = 1;
			}else if (pm[0] < sell && pm[1] > sell) {
				last_signal = 1;
			}
			//Modify profits
			if(buy_at != 0.0){ //Sell bought
				if (last_signal == 2) {
					profit *= ohlc[x][3]/buy_at;
					profit *= 0.9946;
					buy_at = 0.0;
				}
			}else if(last_signal == 0){ //Buy
				buy_at = ohlc[x][3];
				profit *= 0.9946;
			}
		}
	}
	results[ind].signal = last_signal;
	results[ind].profit = profit;
}