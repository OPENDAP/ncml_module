Attributes {
    NC_GLOBAL {
        String projection_type "mapped";
        String projection "geographic";
        Int32 gctp_sys 0;
        Int32 gctp_zone 0;
        Float64 gctp_parm 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000, 0.0000000000000000;
        Int32 gctp_datum 12;
        Float32 Southernmost_Northing 37.26869965;
        Float32 Northernmost_Northing 38.02470016;
        Float32 Westernmost_Easting 236.5800018;
        Float32 Easternmost_Easting 237.4799957;
        Float64 et_affine 0.0000000000000000, 0.027000000000000007, 0.036000000000000226, 0.0000000000000000, 236.58000000000001, 37.268700000000003;
        Int32 rows 29;
        Int32 cols 26;
        Float64 polygon_latitude 37.268700000000003, 38.024700000000003, 38.024700000000003, 37.268700000000003, 37.268700000000003;
        Float64 polygon_longitude 236.58000000000001, 236.58000000000001, 237.48000000000002, 237.48000000000002, 236.58000000000001;
        String time_coverage_end "2006-06-07T12:00:00Z";
    }
    CGusfc {
        Float32 _FillValue -1.000000033e+32;
        Float32 missing_value -1.000000033e+32;
        Int32 numberOfObservations 303;
        Float32 actual_range -0.2876400054, 0.2763200104;
        fileTime {
            String _CoordinateAxisType "Time";
        }
        CGusfc {
        }
        time {
            String long_name "End Time";
            String standard_name "time";
            String units "seconds since 1970-01-01T00:00:00Z";
            Float64 actual_range 1149681600.0000000, 1149681600.0000000;
        }
        altitude {
            String long_name "Altitude";
            String standard_name "altitude";
            String units "m";
            Float32 actual_range 0.000000000, 0.000000000;
        }
        lat {
            String long_name "Latitude";
            String standard_name "latitude";
            String units "degrees_north";
            String point_spacing "even";
            Float32 actual_range 37.26869965, 38.02470016;
            String coordsys "geographic";
        }
        lon {
            String long_name "Longitude";
            String standard_name "longitude";
            String units "degrees_east";
            String point_spacing "even";
            Float32 actual_range 236.5800018, 237.4799957;
            String coordsys "geographic";
        }
    }
    fileTime {
        String _CoordinateAxisType "Time";
    }
}
