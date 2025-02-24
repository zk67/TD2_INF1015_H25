﻿//*** Solutionnaire version 2, sans les //[ //] au bon endroit car le code est assez différent du code fourni.
#pragma region "Includes"//{
#define _CRT_SECURE_NO_WARNINGS // On permet d'utiliser les fonctions de copies de chaînes qui sont considérées non sécuritaires.

#include "structures.hpp"      // Structures de données pour la collection de films en mémoire.

#include "bibliotheque_cours.hpp"
#include "verification_allocation.hpp" // Nos fonctions pour le rapport de fuites de mémoire.
#include <memory>
#include <iostream>
#include <fstream>
#include <string>
#include <limits>
#include <algorithm>
#include "cppitertools/range.hpp"
#include <span>
#include "debogage_memoire.hpp"        // Ajout des numéros de ligne des "new" dans le rapport de fuites.  Doit être après les include du système, qui peuvent utiliser des "placement new" (non supporté par notre ajout de numéros de lignes).
using namespace std;
using namespace iter;

#pragma endregion//}

typedef uint8_t UInt8;
typedef uint16_t UInt16;

#pragma region "Fonctions de base pour lire le fichier binaire"//{

UInt8 lireUint8(istream& fichier)
{
	UInt8 valeur = 0;
	fichier.read((char*)&valeur, sizeof(valeur));
	return valeur;
}
UInt16 lireUint16(istream& fichier)
{
	UInt16 valeur = 0;
	fichier.read((char*)&valeur, sizeof(valeur));
	return valeur;
}
string lireString(istream& fichier)
{
	string texte;
	texte.resize(lireUint16(fichier));
	fichier.read((char*)&texte[0], streamsize(sizeof(texte[0])) * texte.length());
	return texte;
}

#pragma endregion//}

//TODO: Une fonction pour ajouter un Film à une ListeFilms, le film existant déjà; on veut uniquement ajouter le pointeur vers le film existant.  Cette fonction doit doubler la taille du tableau alloué, avec au minimum un élément, dans le cas où la capacité est insuffisante pour ajouter l'élément.  Il faut alors allouer un nouveau tableau plus grand, copier ce qu'il y avait dans l'ancien, et éliminer l'ancien trop petit.  Cette fonction ne doit copier aucun Film ni Acteur, elle doit copier uniquement des pointeurs.
//[
void ListeFilms::changeDimension(int nouvelleCapacite)
{
	Film** nouvelleListe = new Film*[nouvelleCapacite];
	
	if (elements != nullptr) {  // Noter que ce test n'est pas nécessaire puique nElements sera zéro si elements est nul, donc la boucle ne tentera pas de faire de copie, et on a le droit de faire delete sur un pointeur nul (ça ne fait rien).
		nElements = min(nouvelleCapacite, nElements);
		for (int i : range(nElements))
			nouvelleListe[i] = elements[i];
		delete[] elements;
	}
	
	elements = nouvelleListe;
	capacite = nouvelleCapacite;
}

void ListeFilms::ajouterFilm(Film* film)
{
	if (nElements == capacite)
		changeDimension(max(1, capacite * 2));
	elements[nElements++] = film;
}

//]

//TODO: Une fonction pour enlever un Film d'une ListeFilms (enlever le pointeur) sans effacer le film; la fonction prenant en paramètre un pointeur vers le film à enlever.  L'ordre des films dans la liste n'a pas à être conservé.
//[
// On a juste fait une version const qui retourne un span non const.  C'est valide puisque c'est la struct qui est const et non ce qu'elle pointe.  Ça ne va peut-être pas bien dans l'idée qu'on ne devrait pas pouvoir modifier une liste const, mais il y aurais alors plusieurs fonctions à écrire en version const et non-const pour que ça fonctionne bien, et ce n'est pas le but du TD (il n'a pas encore vraiment de manière propre en C++ de définir les deux d'un coup).
span<Film*> ListeFilms::enSpan() const { return span(elements, nElements); }

void ListeFilms::enleverFilm(const Film* film)
{
	for (Film*& element : enSpan()) {  // Doit être une référence au pointeur pour pouvoir le modifier.
		if (element == film) {
			if (nElements > 1)
				element = elements[nElements - 1];
			nElements--;
			return;
		}
	}
}
//]

//TODO: Une fonction pour trouver un Acteur par son nom dans une ListeFilms, qui retourne un pointeur vers l'acteur, ou nullptr si l'acteur n'est pas trouvé.  Devrait utiliser span.
//[
// Voir la NOTE ci-dessous pourquoi Acteur* n'est pas const.  Noter que c'est valide puisque c'est la struct uniquement qui est const dans le paramètre, et non ce qui est pointé par la struct.
span<shared_ptr<Acteur>> ListeActeurs::spanListeActeurs() const { return span<shared_ptr<Acteur>>(ptrActeurs.get(), nElements); }

//NOTE: Doit retourner un Acteur modifiable, sinon on ne peut pas l'utiliser pour modifier l'acteur tel que demandé dans le main, et on ne veut pas faire écrire deux versions.
shared_ptr<Acteur> ListeFilms::trouverActeur(const string& nomActeur) const
{
	for (const Film* film : enSpan()) {
		for (const shared_ptr<Acteur>& acteur : (film->acteurs).spanListeActeurs()) {
			if (acteur->nom == nomActeur)
				return acteur;
		}
	}
	return nullptr;
}
//]

//TODO: Compléter les fonctions pour lire le fichier et créer/allouer une ListeFilms.  La ListeFilms devra être passée entre les fonctions, pour vérifier l'existence d'un Acteur avant de l'allouer à nouveau (cherché par nom en utilisant la fonction ci-dessus).
Acteur* lireActeur(istream& fichier//[
, ListeFilms& listeFilms//]
)
{
	Acteur acteur = {};
	acteur.nom            = lireString(fichier);
	acteur.anneeNaissance = lireUint16 (fichier);
	acteur.sexe           = lireUint8  (fichier);
	//[
	Acteur* acteurExistant = listeFilms.trouverActeur(acteur.nom);
	if (acteurExistant != nullptr)
		return acteurExistant;
	else {
		cout << "Création Acteur " << acteur.nom << endl;
		return new Acteur(acteur);
	}
	//]
	return {}; //TODO: Retourner un pointeur soit vers un acteur existant ou un nouvel acteur ayant les bonnes informations, selon si l'acteur existait déjà.  Pour fins de débogage, affichez les noms des acteurs crées; vous ne devriez pas voir le même nom d'acteur affiché deux fois pour la création.
}

Film* lireFilm(istream& fichier//[
, ListeFilms& listeFilms//]
)
{
	Film* film = {};
	film.titre       = lireString(fichier);
	film.realisateur = lireString(fichier);
	film.anneeSortie = lireUint16 (fichier);
	film.recette     = lireUint16 (fichier);
	film.acteurs.nElements = lireUint8 (fichier);  //NOTE: Vous avez le droit d'allouer d'un coup le tableau pour les acteurs, sans faire de réallocation comme pour ListeFilms.  Vous pouvez aussi copier-coller les fonctions d'allocation de ListeFilms ci-dessus dans des nouvelles fonctions et faire un remplacement de Film par Acteur, pour réutiliser cette réallocation.
	//[
	Film* filmp = new Film( film ); //NOTE: On aurait normalement fait le "new" au début de la fonction pour directement mettre les informations au bon endroit; on le fait ici pour que le code ci-dessus puisse être directement donné aux étudiants sans qu'ils aient le "new" déjà écrit.
	cout << "Création Film " << film.titre << endl;
	filmp->acteurs.elements = new Acteur*[filmp->acteurs.size()];

	for (Acteur*& acteur : (filmp->acteurs).spanListeActeurs()) {
		acteur = 
		//]
		lireActeur(fichier//[
		, listeFilms//]
		); //TODO: Placer l'acteur au bon endroit dans les acteurs du film.
		//TODO: Ajouter le film à la liste des films dans lesquels l'acteur joue.
	//[
		acteur->joueDans.ajouterFilm(filmp);
	//]
	}
	//[
	return filmp;
	//]
	return {}; //TODO: Retourner le pointeur vers le nouveau film.
}

ListeFilms::ListeFilms(const string& nomFichier) : possedeLesFilms_(true)
{
	ifstream fichier(nomFichier, ios::binary);
	fichier.exceptions(ios::failbit);
	
	int nElements = lireUint16(fichier);

	//TODO: Créer une liste de films vide.
	//[
	/*
	//]
	for (int i = 0; i < nElements; i++) {
		//[
	*/
	for ([[maybe_unused]] int i : range(nElements)) { //NOTE: On ne peut pas faire un span simple avec spanListeFilms car la liste est vide et on ajoute des éléments à mesure.
		ajouterFilm(
		//]
		lireFilm(fichier//[
		, *this  //NOTE: L'utilisation explicite de this n'est pas dans la matière indiquée pour le TD2.
		//]
		)//[
		)
		//]
		; //TODO: Ajouter le film à la liste.
	}
	
	//[
	/*
	//]
	return {}; //TODO: Retourner la liste de films.
	//[
	*/
	//]
}

//TODO: Une fonction pour détruire un film (relâcher toute la mémoire associée à ce film, et les acteurs qui ne jouent plus dans aucun films de la collection).  Noter qu'il faut enleve le film détruit des films dans lesquels jouent les acteurs.  Pour fins de débogage, affichez les noms des acteurs lors de leur destruction.
//[
void detruireActeur(Acteur* acteur)
{
	cout << "Destruction Acteur " << acteur->nom << endl;
	delete acteur;
}
bool joueEncore(const Acteur* acteur)
{
	return acteur->joueDans.size() != 0;
}
void detruireFilm(Film* film)
{
	for (Acteur* acteur : (film->acteurs).spanListeActeurs()) {
		acteur->joueDans.enleverFilm(film);
		if (!joueEncore(acteur))
			detruireActeur(acteur);
	}
	cout << "Destruction Film " << film->titre << endl;
	//pointeur intelligent se supprime tout seul
	delete film;
}
//]

//TODO: Une fonction pour détruire une ListeFilms et tous les films qu'elle contient.
//[
//NOTE: Attention que c'est difficile que ça fonctionne correctement avec le destructeur qui détruit la liste.  Mon ancienne implémentation utilisait une méthode au lieu d'un destructeur.  Le problème est que la matière pour le constructeur de copie/move n'est pas dans le TD2 mais le TD3, donc si on copie une liste (par exemple si on la retourne de la fonction creerListe) elle sera incorrectement copiée/détruite.  Ici, creerListe a été converti en constructeur, qui évite ce problème.
ListeFilms::~ListeFilms()
{
	if (possedeLesFilms_)
		for (Film* film : enSpan())
			detruireFilm(film);
	delete[] elements;
}
//]

void afficherActeur(const Acteur& acteur, ostream& os)
{
	cout << "  " << acteur.nom << ", " << acteur.anneeNaissance << " " << acteur.sexe << endl;
}

//TODO: Une fonction pour afficher un film avec tous ces acteurs (en utilisant la fonction afficherActeur ci-dessus).
//[
//void afficherFilm(const Film& film)
ostream& operator<<(ostream& os, const Films& film)
{
	os << "Titre: " << film.titre << endl;
	os << "  Réalisateur: " << film.realisateur << "  Année :" << film.anneeSortie << endl;
	os << "  Recette: " << film.recette << "M$" << endl;

	os << "Acteurs:" << endl;
	for (const shared_ptr<Acteur>& acteur : (film.acteurs).spanListeActeurs()) {
		afficherActeur(*acteur, os);
	}
	return os;
}
//]

void afficherListeFilms(const ListeFilms& listeFilms)
{
	//TODO: Utiliser des caractères Unicode pour définir la ligne de séparation (différente des autres lignes de séparations dans ce progamme).
	static const string ligneDeSeparation = //[
		"\033[32m────────────────────────────────────────\033[0m\n";
		/*
		//]
		{};
	//[ */
	//]
	cout << ligneDeSeparation;
	//TODO: Changer le for pour utiliser un span.
	//[
	/*//]
	for (int i = 0; i < listeFilms.nElements; i++) {
		//[*/
	for (const Film* film : listeFilms.enSpan()) {
		//]
		//TODO: Afficher le film.
		//[
		afficherFilm(*film);
		//]
		cout << ligneDeSeparation;
	}
}

void afficherFilmographieActeur(const ListeFilms& listeFilms, const string& nomActeur)
{
	//TODO: Utiliser votre fonction pour trouver l'acteur (au lieu de le mettre à nullptr).
	const Acteur* acteur = //[
		listeFilms.trouverActeur(nomActeur);
		/* //]
		nullptr;
	//[ */
	//]
	if (acteur == nullptr)
		cout << "Aucun acteur de ce nom" << endl;
	else
		afficherListeFilms(acteur->joueDans);
}

int main()
{
	#ifdef VERIFICATION_ALLOCATION_INCLUS
	bibliotheque_cours::VerifierFuitesAllocations verifierFuitesAllocations;
	#endif
	bibliotheque_cours::activerCouleursAnsi();  // Permet sous Windows les "ANSI escape code" pour changer de couleurs https://en.wikipedia.org/wiki/ANSI_escape_code ; les consoles Linux/Mac les supportent normalement par défaut.

	int* fuite = new int; //TODO: Enlever cette ligne après avoir vérifié qu'il y a bien un "Detected memory leak" de "4 bytes" affiché dans la "Sortie", qui réfère à cette ligne du programme.

	static const string ligneDeSeparation = "\n\033[35m════════════════════════════════════════\033[0m\n";

	//TODO: Chaque TODO dans cette fonction devrait se faire en 1 ou 2 lignes, en appelant les fonctions écrites.

	//TODO: La ligne suivante devrait lire le fichier binaire en allouant la mémoire nécessaire.  Devrait afficher les noms de 20 acteurs sans doublons (par l'affichage pour fins de débogage dans votre fonction lireActeur).
	ListeFilms listeFilms("films.bin");
	
	cout << ligneDeSeparation << "Le premier film de la liste est:" << endl;
	//TODO: Afficher le premier film de la liste.  Devrait être Alien.
	//[
	afficherFilm(*listeFilms.enSpan()[0]);
	//]

	cout << ligneDeSeparation << "Les films sont:" << endl;
	//TODO: Afficher la liste des films.  Il devrait y en avoir 7.
	//TODO: Afficher la liste des films.  Il devrait y en avoir 7.
	//[
	afficherListeFilms(listeFilms);
	//]

	//TODO: Modifier l'année de naissance de Benedict Cumberbatch pour être 1976 (elle était 0 dans les données lues du fichier).  Vous ne pouvez pas supposer l'ordre des films et des acteurs dans les listes, il faut y aller par son nom.
	//[
	listeFilms.trouverActeur("Benedict Cumberbatch")->anneeNaissance = 1976;
	//]

	cout << ligneDeSeparation << "Liste des films où Benedict Cumberbatch joue sont:" << endl;
	//TODO: Afficher la liste des films où Benedict Cumberbatch joue.  Il devrait y avoir Le Hobbit et Le jeu de l'imitation.
	//[
	afficherFilmographieActeur(listeFilms, "Benedict Cumberbatch");
	//]
	
	//TODO: Détruire et enlever le premier film de la liste (Alien).  Ceci devrait "automatiquement" (par ce que font vos fonctions) détruire les acteurs Tom Skerritt et John Hurt, mais pas Sigourney Weaver puisqu'elle joue aussi dans Avatar.
	//[
	detruireFilm(listeFilms.enSpan()[0]);
	listeFilms.enleverFilm(listeFilms.enSpan()[0]);
	//]

	cout << ligneDeSeparation << "Les films sont maintenant:" << endl;
	//TODO: Afficher la liste des films.
	//[
	afficherListeFilms(listeFilms);
	//]

	//TODO: Faire les appels qui manquent pour avoir 0% de lignes non exécutées dans le programme (aucune ligne rouge dans la couverture de code; c'est normal que les lignes de "new" et "delete" soient jaunes).  Vous avez aussi le droit d'effacer les lignes du programmes qui ne sont pas exécutée, si finalement vous pensez qu'elle ne sont pas utiles.
	//[
	// Les lignes à mettre ici dépendent de comment ils ont fait leurs fonctions.  Dans mon cas:
	//listeFilms.enleverFilm(nullptr); // Enlever un film qui n'est pas dans la liste (clairement que nullptr n'y est pas).
	//afficherFilmographieActeur(listeFilms, "N'existe pas"); // Afficher les films d'un acteur qui n'existe pas.
	//]

	//TODO: Détruire tout avant de terminer le programme.  L'objet verifierFuitesAllocations devrait afficher "Aucune fuite detectee." a la sortie du programme; il affichera "Fuite detectee:" avec la liste des blocs, s'il manque des delete.
//[
//]
}
