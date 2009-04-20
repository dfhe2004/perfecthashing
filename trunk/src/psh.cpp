#include "psh.h"




PSH::PSH(CImg<unsigned char> image) {
    this->image = image;

    //Random values generator initialization
	srand(time(NULL));

	assert(image.width == image.height);			//We only use square images !

}



int PSH::perfect_hashing(CImg<unsigned char> &hash, CImg<unsigned char> &offsets, int &n, int &m, int &u, int &r){
    /**
     * Main algorithm
     * @image : original image
     * @hash  : hash table
     * @offsets : offset table
     * @n : non-0 data size
     * @m : hash table size
     * @u : original image size
     * @r : offset table size
     **/

	//Hashing table initialization
	hash.fill(255);

	//Offset table declaration
	assert((u <= m*r) && (pgcd(m,r) == 1));								// Otherwise no injectivity of S -> (h0, h1) !
	offsets.assign(r, r, 1, 2, 0);
	cout << "Offset table : " << r << "x" << r << endl;

	//Calcul des cardinaux des ensembles h1^(-1)(q)
	CImg<int> cards(r, r, 1, 1, 0);
	for (int i=0; i<u; ++i)
	{
		for (int j=0; j<u; ++j)
		{
			if (image(i,j) != 255)
				cards(i%r, j%r)++;
		}
	}

	////Verification (sum over all coefficients of cards should be equal to n)
	//int sum = 0;
	//for (int i=0; i<r; ++i)
	//{
	//	for (int j=0; j<r; ++j)
	//	{
	//		sum += cards(i,j);
	//	}
	//}
	//cout << endl << "SUM: " << sum << " Correct value: " << n << endl;


	//Sort cardinals
	CImg<int> perms = cards;
	perms.fill(0);
	CImg<int> sort = cards.get_sort(perms, false);


	//Loop over sorted cardinals
	if (VERBOSE)
	{
		cout << "---------------------------------------------" << endl;
		cout << "               Main loop" << endl;
		cout << "---------------------------------------------" << endl;
	}
	std::vector<Point> points;	//Positions in hash table
	std::vector<Point> valid_translations;
	int nb_points = 0;
	for (int j=0; j<r; ++j)
	{
		for (int i=0; i<r; ++i)
		{
			int pos = perms(i,j);

			//Position in the offset table
			int x = pos%r;
			int y = pos/r;

			if (VERBOSE)
				cout << "Position in offset table : " << x << " " << y << endl;

			//Search all points of h^(-1)(x,y)
			points.clear();
			for (int k=x; k<u; k+=r)
			{
				for (int l=y; l<u; l+=r)
				{
					unsigned char col = image(k,l);
					if (col != 255)
						points.push_back(Point(k%m, l%m, col));
				}
			}
			if (VERBOSE)
				cout << "\t" << points.size() << " points found" << endl;
			nb_points += points.size();

			//Search for possible translations (the ones with no collisions)
			valid_translations.clear();
			for (int k=0; k<m; ++k)
			{
				for (int l=0; l<m; ++l)
				{
					// Is (k,l) a valid translation?
					bool valid = true;
					for (unsigned int s=0; s<points.size(); ++s)
					{
						Point p = points[s];
						if (hash((p.x+k)%m, (p.y+l)%m) != 255)
						{
							valid = false;
							break;
						}
					}

					if (valid)
						valid_translations.push_back(Point(k,l));
				}
			}
			if (VERBOSE)
				cout << "\t" << valid_translations.size() << " valid translations found" << endl;

			//No valid translation found !!
			if (valid_translations.size() == 0)
			{
				cout << nb_points << "/" << n << " points compressed..." << endl;
				return 0;
			}

			//Random selection of a valid translation
			int rand_index = rand()%valid_translations.size();
			Point offset = valid_translations[rand_index];
			if (VERBOSE)
				cout << "Translation taken: " << offset.x << " " << offset.y << endl;

			//Offset assignment
			int off_x = offset.x;
			int off_y = offset.y;
			offsets(x,y,0,0) = off_x;
			offsets(x,y,0,1) = off_y;

			//Hash table assignment
			for (unsigned int s=0; s<points.size(); ++s)
			{
				Point p = points[s];
				hash((p.x+off_x)%m, (p.y+off_y)%m) = p.color;
			}

			if (VERBOSE)
			{
				cout << endl;
				getchar();
			}
		}
	}

	cout << "Perfect hashing succeeded !!" << endl;

	return 1;
}





int PSH::perform()
{
    int u = image.width;

	//Finds number of non-0 points
	int n = 0;
	for (int i=0; i<u; ++i)
	{
		for (int j=0; j<u; ++j)
		{
			if (image(i,j) != 255)
				n++;
		}
	}

	cout << "Image " << u << "x" << u << "  " << n << "/" << u*u << " non white pixels." << endl;


	//Hashing table declaration
	int m = ceil(sqrt((double) n));
	cout << "Hashing table : " << m << "x" << m << endl << endl;
	CImg<unsigned char> hash(m, m, 1, 1, 255);

	//Offset table declaration
	CImg<unsigned char> offsets;

	//Initialization of r
	int r = ceil(sqrt(1.*n/4));
	while (pgcd(r, m) != 1)
		r++;

#ifndef COMPACT
	//Geometric increase of r
	while (perfect_hashing(hash, offsets, n, m, u, r) == 0)
	{
		cout << endl;
		r = ceil(r*1.2);
		while (pgcd(r, m) != 1)
			r++;
	}
	cout << endl << "Optimal offset table size for compact construction: r=" << r << endl;
#else
	//Dichotomic search (we want to find the lowest acceptable r so that perfect hashing succeeds)
	int r_min = 2;
	while (pgcd(r_min, m) != 1)
		r_min++;

	int best_r = INT_MAX;
	CImg<unsigned char> best_hash;
	CImg<unsigned char> best_offset;

	bool finished = false;
	while (!finished)
	{
		//Test of success with r
		bool success = false;
		int i;
		for (i=0; i<MAX_ESSAIS_COMPACT; ++i)
		{
			if (perfect_hashing(image, hash, offsets, n, m, u, r) == 1)
			{
				success = true;
				cout << endl;
				break;
			}
			else
				continue;
		}
		if (i == 5)
			cout << endl;

		//Ca n'a pas marche
		if (!success)
		{
			r_min = r;

			//Si on n'a pas encore trouvé de bon r, on augmente r
			if (best_r == INT_MAX)
			{
				r = ceil(r*1.2);
				while (pgcd(r, m) != 1)
					r++;
			}
			//Sinon, on fait la moyenne entre le r courant et le best_r qui marche
			else
			{
				int old_r = r;
				r = (best_r + r)/2;
				int mid_r = r;
				while ((pgcd(r, m) != 1) && (r != old_r))
					r--;
				if (r == old_r)
				{
					r = mid_r;
					while ((pgcd(r, m) != 1) && (r != best_r))
						r++;
					if (r == best_r || r == old_r)
					{
						cout << "Optimal offset table size for compact construction: r=" << best_r << endl;
						finished = true;
					}
				}

			}

		}

		//Si ça a marche
		else
		{
			int old_r = r;
			r = (r_min + r)/2;
			int mid_r = r;
			while ((pgcd(r, m) != 1) && (r != r_min))
				r--;
			if (r == r_min)
			{
				r = mid_r;
				while ((pgcd(r, m) != 1) && (r != old_r))
					r++;
				if (r == old_r || r == r_min)
				{
					cout << "Optimal offset table size for compact construction: r=" << old_r << endl;
					finished = true;
				}
			}

			best_r = old_r;
			best_offset = offsets;
			best_hash = hash;
		}


	}

#endif


    hash.display("Hash Table");


    offsets.display("Offset Table");

    if (SAVE_OUTPUT) {
        hash.save("../Output/Hash Table.bmp");
        offsets.save("../Output/Offset Table.bmp");
    }

	//getchar();
	return 0;
}
