clc;
clear;
close all;

% Available high-voltage resistor values from Yageo:
% 1.5M, 1.62M, 2.0M, 3.3M, 5.6M, 10M

% HV detector
R_H_DET =  [9.1e6,  10e6, 5.6e6,   10e6, 5.6e6, 5.6e6, 3.3e6, 3.3e6];
num_R_H_DET =  [4,     6,     6,      4,     5,     5,     8,     5];
R_L_DET =  [1.3e6, 2.0e6, 1.0e6,  1.5e6, 1.0e6, 1.0e6, 910e3, 560e3];

% Measurement
R_H_MEAS = [1.8e6, 1.5e6, 1.5e6, 1.62e6, 2.0e6, 2.0e6, 2.0e6, 3.3e6];
num_R_H_MEAS = [4,     4,     5,      4,     4,     4,     4,     4];
R_L_MEAS = [7.5e3, 6.2e3, 7.5e3,  6.8e3, 8.2e3, 8.0e3, 8.2e3,  13e3];

% Input resistance
R_IN_DET = R_H_DET .* num_R_H_DET + R_L_DET;
R_IN_MEAS = 2 .* (R_H_MEAS .* num_R_H_MEAS + R_L_MEAS);
R_IN = 1 ./ (1 ./ R_IN_DET + 1 ./ R_IN_MEAS);

% Threshold voltage HV detection at 50V
V_DET_LIM = 50;
V_DET_TH = V_DET_LIM ./ (R_H_DET .* num_R_H_DET + R_L_DET) .* R_L_DET;

% Measurement gain
G_MEAS = 1 ./ (R_H_MEAS .* num_R_H_MEAS + R_L_MEAS) .* R_L_MEAS;

% Print results
disp(["Bat_source Term input resistor analyis"]);
disp(["======================================"]);
for sol=1:numel(R_IN)
  disp("");
  disp(["HV detector high resitor value:    " num2str(num_R_H_DET(sol))   " x " num2str(R_H_DET(sol)/1e6)   " MOhm"]);
  disp(["HV detector low resitor value:     " num2str(R_L_DET(sol)/1e6)   " MOhm"]);
  %disp("");
  disp(["Measurement high resitor value:    " num2str(num_R_H_MEAS(sol))  " x " num2str(R_H_MEAS(sol)/1e6)  " MOhm"]);
  disp(["Measurement low resitor value:     " num2str(R_L_MEAS(sol)/1e3)  " kOhm"]);
  disp(["..............................................."]);
  %disp("");
  disp(["Input resistance HV detector:      " num2str(R_IN_DET(sol)/1e6)  " MOhm"]);
  disp(["Input resistance Measurement:      " num2str(R_IN_MEAS(sol)/1e6) " MOhm"]);
  %disp("");
  disp(["Input resistance:                  " num2str(R_IN(sol)/1e6)      " MOhm"]);
  %disp("");
  disp(["HV detector threshold:             " num2str(V_DET_TH(sol))      " V"]);
  %disp("");
  disp(["Measurement gain:                  " num2str(G_MEAS(sol))        " "]);
  disp(["-----------------------------------------------"]);
  disp("");
endfor
