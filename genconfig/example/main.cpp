#include <stdio.h>
#include <toml.hpp>
#include <cnode.h>

#define DEFINE_CONFIG_TREE
#include "config.h"

const char *config_file_in_path = "config.toml";
const char *config_file_out_path = "config_out.toml";

void add_supplier(struct config *s, int product, const char *name)
{
	int c = s->company.products[product].suppliers_count;
	s->company.products[product].suppliers = (const char **)reallocarray(
		s->company.products[product].suppliers, c + 1, sizeof(char *));
	s->company.products[product].suppliers[c] = name;
	s->company.products[product].suppliers_count = c + 1;
}

void set_product_price(struct config *s, int product, float price)
{
	s->company.products[product].price = price;
}

int main(int argc, char *argv[])
{
	struct config s;

	// Load config from user file
	auto toml_table = toml::parse_file(config_file_in_path);
	config_tree.update_from_table(toml_table);
	config_tree.store_to_struct(&s);

	// Work with config
	printf("Company is %s headquartered in %s, %s\n",
		   s.company.name,
		   s.company.headquarters.city,
		   s.company.headquarters.state);

	puts("Available products:");
	for (int i = 0; i < s.company.products_count; i++) {
		printf(" - %s %d units @ $%.2f ea.",
			   s.company.products[i].name,
			   s.company.products[i].inventory,
			   s.company.products[i].price);
		if (s.company.products[i].international_shipping)
			printf(" *International*");
		if (s.company.products[i].category == CONFIG_COMPANY_PRODUCTS_CATEGORY_EXPLOSIVE)
			printf(" *Hazardous*");

		puts("");

		if (s.company.products[i].suppliers_count) {
			puts("   Supplied by:");
			for (int j = 0; j < s.company.products[i].suppliers_count; j++)
				printf("   - %s\n", s.company.products[i].suppliers[j]);
		}
	}

	// Update some config
	// config_tree.set_defaults();
	add_supplier(&s, 0, "Fred's Apples LLC");
	set_product_price(&s, 1, 995.75);

	// Sync config from structure
	config_tree.update_from_struct(&s);
	// config_tree.repr();

	// Save config
	FILE *f = fopen(config_file_out_path, "wb");
	assert(f != NULL);
	fprintf(f, "%s", config_tree.generate_delta_toml().c_str());
	fclose(f);

	return 0;
}
