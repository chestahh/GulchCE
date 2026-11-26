## GulchCE - GET YOUR SHADOWS (and dynamic lighting) HERE!

[GET-YOUR-SHADOWS-HERE](https://github.com/chestahh/GulchCE/tree/GET-YOUR-SHADOWS-HERE) brings GulchCE’s dynamic lighting, modern shadows and shared renderer optimisations to OpenCE-based forks. It works with existing Xbox, Custom Edition and MCC maps using the lighting and geometry already in the map.

- **Dynamic lighting.** Plasma bolts and grenades carry their light through the scene. Kinetic and plasma muzzleflashes follow the firing weapon, and shields, pickups and vehicle engines illuminate nearby surfaces. A passing plasma bolt can light one side of a Marine, then throw his silhouette across the opposite wall as it flies past.

- **Local shadows.** Each selected light gets its own depth map, so two lights can cast the same object’s shadow in different directions. Hard preserves crisp silhouettes. Soft filters the depth comparisons to smooth the edges while keeping the caster’s shape. Stable selection keeps shadows attached to their lights during combat.

- **Sun and distant shadows.** The renderer reads the sun direction from the map’s active sky. Characters, vehicles, scenery and level geometry cast along that direction. Nearby shadows retain fine detail, distant coverage fades smoothly, and scoping in brings the area you’re aiming at into focus. Sunlight has its own rendering budget.
  
- **Map-authored fixtures.** Eligible lamps and emitters cast independently using the map’s authored lighting. Walk between the two cyan lamps on Rat Race and each lamp can cast its own shadow. The moving shadows blend with the room’s baked lighting, preserving the mapper’s original look.
  
- **Flashlight and weapon shadows.** The flashlight projects shadows down its beam, so a Grunt or crate can cast onto the floor or wall behind it. Weapon lights account for who is firing and which camera is active. The shooter stays lit in first person, while third person retains the biped’s muzzleflash silhouette.
  
- **Shared renderer optimisations.** Map-load shader preparation moves work into the loading screen. The renderer reuses collision results and geometry, batches adjacent map surfaces, and records texture opacity at upload time. Solid surfaces take a cheaper depth pass, while foliage and grates retain their cutout silhouettes. **This results in a 150-200fps increase in performance on stock settings in a direct benchmark against OpenCE.** The shared renderer optimisations are not optional and are necesssary for the dynamic lighting & dynamic shadowing system to be performant.

## Gallery

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/f895f574-fa64-4ee3-b9be-50eecd50fdad" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/b155d192-227c-409a-a013-68f2bf019bdf" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/6fd99940-8578-4a5c-b705-ba1f091b4687" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d0853b19-90da-4740-b7c7-6c1fcd8599c7" />
<img width="1280" height="758" alt="image" src="https://github.com/user-attachments/assets/78466010-a5f5-4728-beaf-d006a9450da9" />
<img width="1280" height="589" alt="image" src="https://github.com/user-attachments/assets/485db55b-8ae5-41cc-9030-79c2ca1c3ff8" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/0fd77a58-157e-4f7a-b5a7-c82961b1567d" />
<img width="1280" height="645" alt="image" src="https://github.com/user-attachments/assets/b830b050-4ed3-4be1-b69b-420e687cad21" />
<img width="1280" height="621" alt="image" src="https://github.com/user-attachments/assets/77c3da78-6d4a-4250-871f-d7453469c309" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/fbb0c0e6-ffad-46af-bab4-8e48d1695fa9" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/ea953973-0453-4c91-9226-e76340db5347" />
<img width="1280" height="642" alt="image" src="https://github.com/user-attachments/assets/b8416856-ec9f-48c6-babf-e4c614ff7276" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/a942b01a-af3a-49f7-9416-f77e66f2147e" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/2c045a7a-35ae-424e-81be-99ca795b4c6c" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d48e9edf-75f7-4a70-af3e-7e3ab01d5116" />
