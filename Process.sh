g++ -std=c++0x -o covid_process main.cpp -lpthread && ./covid_process "*" ~/Projects/Covid19/COVID-19/csse_covid_19_data/csse_covid_19_daily_reports/* > Global_Data.csv
g++ -std=c++0x -o covid_process main.cpp -lpthread && ./covid_process "US" ~/Projects/Covid19/COVID-19/csse_covid_19_data/csse_covid_19_daily_reports/* > US_States_Data.csv
g++ -std=c++0x -o covid_process main.cpp -lpthread && ./covid_process "US.California.*" ~/Projects/Covid19/COVID-19/csse_covid_19_data/csse_covid_19_daily_reports/* > California_Data.csv 
