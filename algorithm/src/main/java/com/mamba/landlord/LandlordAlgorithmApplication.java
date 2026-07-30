package com.mamba.landlord;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication(excludeName = "org.springframework.boot.autoconfigure.jdbc.DataSourceAutoConfiguration")
public class LandlordAlgorithmApplication {

	public static void main(String[] args) {
		SpringApplication.run(LandlordAlgorithmApplication.class, args);
	}

}
