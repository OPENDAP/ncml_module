Attributes {
    String title "Union cldc and lflx";
    NC_GLOBAL {
        String title "COADS 1-degree Equatorial Enhanced";
        String history "";
        String Conventions "COARDS";
    }
    DODS_EXTRA {
        String Unlimited_Dimension "time";
    }
    cldc {
        Float32 valid_range 0.000000000, 8.000000000;
        Float32 actual_range 0.000000000, 8.000000000;
        String units "okta";
        Int16 precision 1;
        Int16 missing_value 32766;
        Int16 _FillValue 32766;
        String long_name "Cloudiness Monthly Mean at Surface";
        String dataset "COADS 1-degree Equatorial Enhanced\\012AI";
        String var_desc "Cloudiness\\012C";
        String level_desc "Surface\\0120";
        String statistic "Mean\\012M";
        String parent_stat "Individual Obs\\012I";
        Float32 add_offset 3276.500000;
        Float32 scale_factor 0.1000000015;
        cldc {
        }
        time {
            String units "days since 1-1-1 00:00:0.0";
            String long_name "Time";
            String delta_t "0000-01-00 00:00:00";
            String avg_period "0000-01-00 00:00:00";
            Float64 actual_range 715511.00000000000, 729360.00000000000;
        }
        lat {
            String long_name "Latitude";
            String units "degrees_north";
            Float32 actual_range 10.00000000, -10.00000000;
        }
        lon {
            String long_name "Longitude";
            String units "degrees_east";
            Float32 actual_range 0.5000000000, 359.5000000;
        }
    }
    lflx {
        Float32 valid_range -1000.000000, 1000.000000;
        Float32 actual_range -88.70000458, 236.1000061;
        String units "grams/kg m/s";
        Int16 precision 1;
        Int16 missing_value 32766;
        Int16 _FillValue 32766;
        String long_name "Latent Heat Parameter Monthly Mean at Surface";
        String dataset "COADS 1-degree Equatorial Enhanced\\012AI";
        String var_desc "Latent Heat Parameter\\012G";
        String level_desc "Surface\\0120";
        String statistic "Mean\\012M";
        String parent_stat "Individual Obs\\012I";
        Float32 add_offset 2276.500000;
        Float32 scale_factor 0.1000000015;
        lflx {
        }
        time {
            String units "days since 1-1-1 00:00:0.0";
            String long_name "Time";
            String delta_t "0000-01-00 00:00:00";
            String avg_period "0000-01-00 00:00:00";
            Float64 actual_range 715511.00000000000, 729360.00000000000;
        }
        lat {
            String long_name "Latitude";
            String units "degrees_north";
            Float32 actual_range 10.00000000, -10.00000000;
        }
        lon {
            String long_name "Longitude";
            String units "degrees_east";
            Float32 actual_range 0.5000000000, 359.5000000;
        }
    }
}
