Options de compilation :

LDFIFOFLAG: ajoute le drapeau -DFIFO au préprocesseur lors de la compilation et permet d'utiliser 
                un ordonnancement par file d'attente

LDPRIORITYFLAG: ajoute le drapeau -DPRIORITY au préprocesseur lors de la compilation et permet d'utiliser 
                un ordonnancement par priorité 

LDPREEMPTIONFLAG: ajoute le drapeau -DPREEMPTION au préprocesseur lors de la compilation.


Indications : 

- Les flags de priorité peuvent être rajouté donc via le Makefile ou en ligne de commande. 

- Les flags LDFIFOFLAG et LDPRIORITYFLAG ne peuvent pas être rajouté en même temps car utilisant 
deux files différentes qui portent le même nom dans le fichier.

- Certains tests ne sont pas possibles avec le support multicoeurs. Ce sont: 
le tests de barrier, de conditions, de sémaphore, de débordement de pile et de signaux.
On pourra donc les exclure directement dans le makefile en les rajoutant à EXCLUDED_TESTS. 
Par exemple : EXCLUDED_TESTS = 64-cond.c excluera le test de conditions.

- Il sera également nécessaire d'exclure les tests de fonctionnalité rajouté lors des tests
avec pthread



