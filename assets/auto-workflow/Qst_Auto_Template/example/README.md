# Qst Calc v1.0 - Example Data

This folder contains example multi-temperature adsorption isotherm data for **Qst (isosteric heat of adsorption) calculation**.

## Data Format

CSV files with two columns:

```csv
Pressure_kPa,Uptake_mmol_g
0.1,0.52
0.5,1.85
...
```

- `Pressure_kPa`: Equilibrium pressure in kPa
- `Uptake_mmol_g`: Adsorption amount in mmol/g

## Files

| File | Temperature | Description |
|------|-------------|-------------|
| `CO2_273K.csv` | 273 K | CO₂ adsorption at 0 °C |
| `CO2_298K.csv` | 298 K | CO₂ adsorption at 25 °C |
| `CO2_313K.csv` | 313 K | CO₂ adsorption at 40 °C |

## Usage

1. Launch `qst_calc_gui.exe`
2. Click **Add** to load each CSV file
3. Enter the corresponding temperature (e.g. `273`, `298`, `313`) for each dataset
4. Click **Run** to calculate Qst

## Notes

- At least **2 temperature datasets** are required
- More temperatures improve fitting accuracy and Qst reliability
- Data should cover the same pressure range across all temperatures
